#include "auto_cattery/ui/mew_ui_bridge.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>

#include "auto_cattery/config.hpp"
#include "auto_cattery/furniture_analysis/service.hpp"
#include "auto_cattery/logger.hpp"
#include "auto_cattery/recommendation/snapshot_reader.hpp"
#include "auto_cattery/scoring/combat_ranker.hpp"
#include "auto_cattery/snapshot/save_snapshot_adapter.hpp"
#include "auto_cattery/ui/house_button_controller.hpp"
#include "furniture_placement_gateway.hpp"
#include "furniture_move_probe_controller.hpp"
#include "house_move_probe_controller.hpp"
#include "in_game_panel_controller.hpp"
#include "auto_cattery/ui/recommendation_marker_controller.hpp"
#include "auto_cattery/workflow/organize_workflow_facade.hpp"
#include "mew_ui_house_button_view.hpp"
#include "mew_ui_house_cat_probe.h"
#include "mew_ui_house_detail_adapter.h"
#include "mew_ui_furniture_move_adapter.h"
#include "mew_ui_house_move_adapter.h"
#include "mew_ui_mapping_probe.h"
#include "mew_ui_management_panel_view.hpp"
#include "mew_ui_recommendation_marker_view.hpp"
#include "mew_ui_scene_probe.h"
#include "runtime_house_move_gateway.hpp"
#include "runtime_house_state_capture.hpp"
#include "runtime_matched_save_snapshot_adapter.hpp"
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#include "mew_ui_api.h"

namespace autocattery::ui {
namespace {

constexpr std::size_t kSceneProbeCapacity = 64;
constexpr std::size_t kMappingRecordCapacity = 128;
constexpr std::size_t kFurnitureSnapshotCapacity = 512;
constexpr std::size_t kFurnitureGridSnapshotCapacity = 32;
constexpr auto kFurnitureTransactionSettleDelay =
    std::chrono::milliseconds(250);
constexpr auto kMappingSnapshotRetryDelay = std::chrono::seconds(1);
constexpr auto kMappingSnapshotRetryWindow = std::chrono::seconds(30);
constexpr std::size_t kMinimumMappedCoverageNumerator = 3;
constexpr std::size_t kMinimumMappedCoverageDenominator = 4;
constexpr auto kFurnitureBuildingComponent = "FurnitureBuildingUI";

Result<RuntimeFurnitureState> CaptureRuntimeFurnitureState(
    void* house_scene_manager) {
    std::array<
        AcMewFurniturePieceSnapshot,
        kFurnitureSnapshotCapacity> snapshots{};
    std::uint8_t complete{};
    const auto count = AcMewEnumerateFurniturePieces(
        house_scene_manager,
        snapshots.data(),
        snapshots.size(),
        &complete);
    if (complete == 0U) {
        return {{}, ErrorCode::SnapshotInvalid,
                "live furniture enumeration was unavailable or truncated"};
    }
    RuntimeFurnitureState state;
    state.scene_piece_count = count;
    state.placements.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const auto& current = snapshots[index];
        if (current.grid == nullptr || current.room[0] == '\0') {
            state.warehouse_pieces.push_back({
                .stable_key = current.stable_key,
                .item_id = current.item});
            continue;
        }
        state.placements.push_back({
            .stable_key = current.stable_key,
            .item_id = current.item,
            .room_id = current.room,
            .position_x = current.saved_x,
            .position_y = current.saved_y,
            .scale_x = static_cast<std::int32_t>(current.scale_x),
            .scale_y = static_cast<std::int32_t>(current.scale_y)});
    }
    std::array<
        AcMewFurnitureGridSnapshot,
        kFurnitureGridSnapshotCapacity> grids{};
    complete = 0U;
    const auto grid_count = AcMewEnumerateFurnitureGrids(
        house_scene_manager,
        grids.data(),
        grids.size(),
        &complete);
    if (complete == 0U || grid_count == 0U) {
        return {{}, ErrorCode::RoomDataUnavailable,
                "live furniture room-grid enumeration was unavailable or truncated"};
    }
    state.room_grids.reserve(grid_count);
    for (std::size_t index = 0; index < grid_count; ++index) {
        const auto& grid = grids[index];
        if (grid.room[0] == '\0' ||
            std::ranges::any_of(
                state.room_grids,
                [&grid](const auto& existing) {
                    return existing.room_id == grid.room;
                })) {
            return {{}, ErrorCode::RoomDataUnavailable,
                    "live furniture room grids were empty or ambiguous"};
        }
        RuntimeFurnitureRoomGridState captured{
            .room_id = grid.room,
            .width = grid.width,
            .height = grid.height};
        const auto cell_count = captured.width * captured.height;
        captured.base_cells.resize(cell_count);
        captured.live_cells.resize(cell_count);
        if (cell_count == 0U ||
            AcMewCopyFurnitureGridCells(
                &grid,
                captured.base_cells.data(),
                captured.live_cells.data(),
                cell_count) == 0) {
            return {{}, ErrorCode::RoomDataUnavailable,
                    "live furniture room-grid cells were unavailable for " +
                        captured.room_id};
        }
        state.room_grids.push_back(std::move(captured));
    }
    return {std::move(state)};
}

bool Contains(const std::vector<std::string>& values, std::string_view value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}

std::string SafeTechnicalName(std::string_view value) {
    std::string output;
    output.reserve(value.size());
    for (const unsigned char character : value) {
        const bool safe =
            std::isalnum(character) != 0 ||
            character == '_' || character == '-' ||
            character == '.' || character == ':' ||
            character == '/';
        output.push_back(safe ? static_cast<char>(character) : '?');
    }
    return output;
}

std::vector<FurnitureWarehouseReplacementRequest::SupportDependent>
BuildSupportDependentRequests(
    const furniture_analysis::FurnitureAttributeUpgrade& upgrade) {
    std::vector<FurnitureWarehouseReplacementRequest::SupportDependent>
        result;
    result.reserve(upgrade.support_dependents_top_down.size());
    for (const auto& dependent : upgrade.support_dependents_top_down) {
        result.push_back({
            .locator = {
                .item = dependent.item_id,
                .preferred_key = dependent.stable_key},
            .room = dependent.room_id,
            .x = dependent.x,
            .y = dependent.y});
    }
    return result;
}

std::string SafeDisplayName(std::string_view value) {
    std::string output(value);
    for (auto& character : output) {
        const auto byte = static_cast<unsigned char>(character);
        if (byte < 0x20 || byte == 0x7F) {
            character = ' ';
        }
    }
    return output.empty() ? "Cat" : output;
}

std::string CompactAttributeGain(
    const snapshot::RoomAttributes& gain) {
    std::ostringstream output;
    output << "C+" << gain.comfort
           << " S+" << gain.stimulation
           << " H+" << gain.health
           << " M+" << gain.mutation
           << " A+" << gain.appeal;
    return output.str();
}

std::string TimestampForFilename() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    localtime_s(&local, &time);
    std::ostringstream output;
    output << std::put_time(&local, "%Y%m%d-%H%M%S");
    return output.str();
}

}  // namespace

struct MewUiBridge::RuntimeScene {
    void* manager{};
    std::string name;
    std::uint32_t component_count{};
    bool ready{};
};

MewUiBridge::MewUiBridge() = default;
MewUiBridge::~MewUiBridge() = default;

const char* MewUiBridge::Name() const noexcept {
    return "MewUiBridge";
}

bool MewUiBridge::Initialize(const InitContext& context) {
    ready_logged_.store(false);
    furniture_mode_ = false;
    furniture_mode_scene_manager_ = nullptr;
    furniture_mode_component_count_ = 0;
    furniture_mode_component_ = nullptr;
    furniture_analysis_preview_.reset();
    furniture_execution_active_ = false;
    furniture_execution_generation_ = 0;
    furniture_execution_upgrade_index_ = 0;
    furniture_execution_index_ = 0;
    furniture_execution_upgraded_ = 0;
    furniture_execution_moved_ = 0;
    furniture_execution_committed_upgrade_indices_.clear();
    furniture_execution_committed_move_indices_.clear();
    furniture_layout_session_generation_ = 0;
    furniture_locked_room_ids_.clear();
    furniture_locked_room_signatures_.clear();
    furniture_retirement_generation_ = 0;
    furniture_retired_keys_.clear();
    furniture_auto_run_active_ = false;
    furniture_auto_run_preview_fresh_ = false;
    furniture_auto_run_upgraded_ = 0;
    furniture_auto_run_moved_ = 0;
    furniture_auto_run_blocked_rooms_ = 0;
    furniture_auto_run_transaction_count_ = 0;
    furniture_auto_run_next_transaction_ = {};
    furniture_attribute_upgrade_committed_in_mode_ = false;
    furniture_room_purposes_.clear();
    furniture_layout_move_tabu_.clear();
    furniture_layout_attempted_state_edges_.clear();
    furniture_focus_room_id_.reset();
    furniture_faulted_generation_ = 0;
    last_tick_time_ = {};
    last_scene_summary_.clear();
    last_house_attach_error_.clear();
    last_recommendation_attach_error_.clear();
    signatures_ = {};
    debug_probe_enabled_ = false;
    diagnostics_root_ = context.mod_root / L"diagnostics";
    recommendation_sidecar_path_ =
        recommendation::RecommendationSidecarPath(context.mod_root);
    mapping_probe_session_.Clear();
    mapping_probe_logged_ = false;
    mapping_identity_logged_ = false;
    mapping_probe_request_sequence_ = 0;
    mapping_snapshot_request_sequence_ = 0;
    mapping_snapshot_task_sequence_ = 0;
    mapping_snapshot_attempt_ = 0;
    mapping_snapshot_generation_ = 0;
    mapping_snapshot_task_generation_ = 0;
    mapping_snapshot_request_active_ = false;
    mapping_snapshot_retry_deadline_ = {};
    mapping_snapshot_next_attempt_ = {};
    next_house_attach_retry_ = {};
    next_recommendation_attach_retry_ = {};
    house_button_view_ = std::make_unique<MewUiHouseButtonView>();
    management_panel_view_ =
        std::make_unique<MewUiManagementPanelView>();
    recommendation_marker_view_ =
        std::make_unique<MewUiRecommendationMarkerView>();
    recommendation_marker_controller_ =
        std::make_unique<RecommendationMarkerController>(
            *recommendation_marker_view_);
    recommendation_marker_controller_->SetRequestHandler(
        [this](std::uint64_t generation) {
            recommendation_detail_targets_.clear();
            const auto historical =
                recommendation::ReadRecommendationSnapshot(
                    recommendation_sidecar_path_);
            const auto message =
                historical.status ==
                        recommendation::SnapshotReadStatus::Missing
                    ? "No committed recommendation sidecar is available; "
                      "anonymous mapping probe armed."
                    : (historical.status ==
                               recommendation::SnapshotReadStatus::Rejected
                           ? "Recommendation sidecar was rejected; anonymous "
                             "mapping probe armed."
                           : "Recommendation sidecar read, but schema 1 lacks "
                             "build/save identity; anonymous mapping probe "
                             "armed.");
            Logger::Instance().Write(
                LogLevel::Info,
                "RecommendationProbe",
                "AC12101",
                "request=" +
                    std::to_string(++mapping_probe_request_sequence_) +
                    " " + message);
            mapping_probe_session_.Arm();
            mapping_probe_logged_ = false;
            mapping_identity_logged_ = false;
            mapping_snapshot_request_sequence_ =
                mapping_probe_request_sequence_;
            mapping_snapshot_generation_ = generation;
            mapping_snapshot_attempt_ = 0;
            mapping_snapshot_request_active_ = true;
            mapping_snapshot_retry_deadline_ =
                std::chrono::steady_clock::now() +
                kMappingSnapshotRetryWindow;
            mapping_snapshot_next_attempt_ = {};
            StartMappingSnapshotAttempt();
        });
    recommendation_marker_controller_->SetDetailsHandler(
        [this](std::uint64_t generation, std::size_t index) {
            const auto context = scene_context_.Current();
            if (context.kind != UiContextKind::House ||
                !context.input_enabled ||
                context.save_in_progress ||
                context.scene_generation != generation ||
                index >= recommendation_detail_targets_.size()) {
                return;
            }
            auto* scene =
                MewUI_GetSceneByName(context.scene_name.c_str());
            const auto opened = AcMewOpenHouseCatDetails(
                scene,
                recommendation_detail_targets_[index]);
            std::ostringstream message;
            message << "rank=" << (index + 1)
                    << " signature=" << (unsigned)opened.signature_valid
                    << " scene=" << (unsigned)opened.scene_valid
                    << " manager=" << (unsigned)opened.click_manager_valid
                    << " drawer=" << (unsigned)opened.drawer_unique
                    << " scene_drawer="
                    << (unsigned)opened.scene_drawer_unique
                    << " drawer_match="
                    << (unsigned)opened.drawer_matches_scene
                    << " cat=" << (unsigned)opened.cat_valid
                    << " target=" << (unsigned)opened.detail_target_valid
                    << " opened=" << (unsigned)opened.invoked
                    << " failure=" << (unsigned)opened.failure_stage
                    << " exception=0x" << std::hex
                    << opened.seh_code
                    << " exception_rva=0x"
                    << opened.exception_rva << std::dec
                    << " box_changed=0 expedition_selection_changed=0";
            Logger::Instance().Write(
                opened.invoked ? LogLevel::Info : LogLevel::Warn,
                "RecommendationMarker",
                "AC12109",
                message.str());
        });

    config_runtime_ = std::make_unique<RuntimeConfigService>(
        context.mod_root / L"config" / L"default_config.json",
        context.mod_root / L"config" / L"user_config.json",
        RuntimeConfigService::Clock{},
        [this](const ConfigInvalidation&) {
            ApplyRuntimeConfig();
        });
    const auto loaded_config = config_runtime_->LoadInitial();
    if (loaded_config.status == ConfigReloadStatus::Rejected) {
        Logger::Instance().Write(
            LogLevel::Error,
            "Config",
            "AC1302",
            "Runtime configuration rejected; safe read-only defaults are active: " +
                loaded_config.message);
    }
    const auto config = config_runtime_->Current();
    const bool english = config.general.language == "en-US";
    house_button_view_->SetEnglish(english);
    recommendation_marker_view_->SetEnglish(english);
    recommendation_scoring_config_ = config.combat_scoring;
    recommendation_marker_config_ = config.recommendation_marker;
    runtime_move_gateway_ =
        std::make_unique<RuntimeHouseMoveGateway>();
    const bool runtime_move_available =
        runtime_move_gateway_->Initialize(
            context.game_root / L"Mewgenics.exe");
    runtime_move_available_ = runtime_move_available;
    furniture_placement_gateway_ =
        std::make_unique<FurniturePlacementGateway>();
    (void)furniture_placement_gateway_->Initialize(
        context.game_root / L"Mewgenics.exe");
    auto runtime_snapshot_adapter =
        std::make_unique<RuntimeMatchedSaveSnapshotAdapter>(
            context.game_root);
    runtime_snapshot_adapter_ = runtime_snapshot_adapter.get();
    furniture_analysis_service_ =
        std::make_unique<furniture_analysis::FurnitureAnalysisService>(
            *runtime_snapshot_adapter_);
    recommendation_marker_controller_->SetFurnitureRequestHandler(
        [this](std::uint64_t generation) {
            StartFurnitureAnalysis(generation);
        });
    organize_workflow_ =
        std::make_unique<workflow::OrganizeWorkflowFacade>(
            std::move(runtime_snapshot_adapter),
            config,
            runtime_move_available
                ? workflow::WorkflowCapability::MoveOnly
                : workflow::WorkflowCapability::PreviewOnly,
            runtime_move_available
                ? runtime_move_gateway_.get()
                : nullptr,
            context.mod_root / L"config",
            context.mod_root / L"AutoCatteryData");
    in_game_panel_controller_ =
        std::make_unique<InGamePanelController>(
            *management_panel_view_, *organize_workflow_,
            context.mod_root, context.game_root);
    house_button_controller_ = std::make_unique<HouseButtonController>(
        *house_button_view_,
        *organize_workflow_,
        HouseButtonController::Clock{},
        [this] {
            RefreshRuntimeSnapshotContext();
        });
    house_button_controller_->SetFurnitureActionHandler(
        [this](std::uint64_t generation) {
            StartFurnitureAutoPlacement(generation);
        });
    house_move_probe_controller_ =
        std::make_unique<HouseMoveProbeController>();
    house_move_probe_controller_->Initialize(
        context.game_root / L"Mewgenics.exe",
        diagnostics_root_);
    furniture_move_probe_controller_ =
        std::make_unique<FurnitureMoveProbeController>();
    furniture_move_probe_controller_->Initialize(
        context.game_root / L"Mewgenics.exe",
        diagnostics_root_);
#ifdef _DEBUG
    debug_probe_enabled_ = true;
#else
    debug_probe_enabled_ =
        config.ui.show_debug_overlay || config.diagnostics.show_debug_overlay;
#endif

    const auto signatures = LoadSceneSignatures(
        context.mod_root / L"config" / L"scene_signatures.json");
    if (signatures) {
        signatures_ = signatures.value;
    } else {
        Logger::Instance().Write(
            LogLevel::Error,
            Name(),
            "AC2201",
            "Scene signatures were rejected; scene detection remains fail-closed.");
    }

    if (!scene_context_.Start()) {
        Logger::Instance().Write(
            LogLevel::Error,
            Name(),
            "AC2202",
            "Scene context service failed to start.");
        return false;
    }
    scene_subscription_ = scene_context_.Subscribe([this](const auto& snapshot) {
        std::ostringstream message;
        message << "Context=" << UiContextKindName(snapshot.kind)
                << " scene='" << snapshot.scene_name
                << "' generation=" << snapshot.scene_generation
                << " input=" << (snapshot.input_enabled ? 1 : 0)
                << " save=" << (snapshot.save_in_progress ? 1 : 0);
        Logger::Instance().Write(
            LogLevel::Info,
            "SceneContext",
            "AC2100",
            message.str());
        if (snapshot.kind != UiContextKind::House ||
            !snapshot.input_enabled ||
            snapshot.save_in_progress) {
            ClearFurnitureLayoutPreview();
            recommendation_detail_targets_.clear();
            mapping_snapshot_request_active_ = false;
            mapping_snapshot_retry_deadline_ = {};
            mapping_snapshot_next_attempt_ = {};
            house_button_controller_->AbandonScene();
            recommendation_marker_controller_->AbandonScene();
            if (in_game_panel_controller_) {
                in_game_panel_controller_->AbandonScene();
            }
            next_house_attach_retry_ = {};
            last_house_attach_error_.clear();
            next_recommendation_attach_retry_ = {};
            last_recommendation_attach_error_.clear();
            if (runtime_move_gateway_) {
                runtime_move_gateway_->SetHouseScene(nullptr);
            }
            if (furniture_placement_gateway_) {
                furniture_placement_gateway_->SetHouseScene(nullptr);
            }
        }
    });

    started_ = MewUI_Start(
        "AutoCattery",
        30,
        100,
        100,
        &MewUiBridge::Tick,
        this) != 0;

    if (started_) {
        // The API's own per-scene callback trace is intentionally disabled:
        // AutoCattery emits a deduplicated summary instead.
        MewUI_SetDebugLogsEnabled(false);
        Logger::Instance().Write(
            LogLevel::Info,
            Name(),
            "AC1200",
            "MewUI bootstrap and phase 02 scene context service started.");
        return true;
    }

    scene_context_.Stop();
    Logger::Instance().Write(
        LogLevel::Error,
        Name(),
        "AC1201",
        "MewUI API bootstrap failed; UI is disabled and the mod is compatibility-degraded.");
    return true;
}

void MewUiBridge::Shutdown() noexcept {
    if (in_game_panel_controller_) {
        in_game_panel_controller_->Detach();
    }
    if (house_button_controller_) {
        house_button_controller_->Detach();
    }
    if (recommendation_marker_controller_) {
        recommendation_marker_controller_->Detach();
    }
    if (house_move_probe_controller_) {
        house_move_probe_controller_->Shutdown();
    }
    if (furniture_move_probe_controller_) {
        furniture_move_probe_controller_->Shutdown();
    }
    if (scene_subscription_ != 0) {
        scene_context_.Unsubscribe(scene_subscription_);
        scene_subscription_ = 0;
    }
    scene_context_.Stop();
    if (started_) {
        MewUI_Stop();
    }
    started_ = false;
    furniture_mode_ = false;
    furniture_mode_scene_manager_ = nullptr;
    furniture_mode_component_count_ = 0;
    furniture_mode_component_ = nullptr;
    furniture_analysis_preview_.reset();
    furniture_execution_active_ = false;
    furniture_execution_generation_ = 0;
    furniture_execution_upgrade_index_ = 0;
    furniture_execution_index_ = 0;
    furniture_execution_upgraded_ = 0;
    furniture_execution_moved_ = 0;
    furniture_execution_committed_upgrade_indices_.clear();
    furniture_execution_committed_move_indices_.clear();
    furniture_layout_session_generation_ = 0;
    furniture_locked_room_ids_.clear();
    furniture_locked_room_signatures_.clear();
    furniture_retirement_generation_ = 0;
    furniture_retired_keys_.clear();
    furniture_auto_run_active_ = false;
    furniture_auto_run_preview_fresh_ = false;
    furniture_auto_run_upgraded_ = 0;
    furniture_auto_run_moved_ = 0;
    furniture_auto_run_blocked_rooms_ = 0;
    furniture_auto_run_transaction_count_ = 0;
    furniture_auto_run_next_transaction_ = {};
    furniture_attribute_upgrade_committed_in_mode_ = false;
    furniture_room_purposes_.clear();
    furniture_layout_move_tabu_.clear();
    furniture_layout_attempted_state_edges_.clear();
    furniture_focus_room_id_.reset();
    furniture_faulted_generation_ = 0;
    last_tick_time_ = {};
    last_scene_summary_.clear();
    last_house_attach_error_.clear();
    last_recommendation_attach_error_.clear();
    next_house_attach_retry_ = {};
    next_recommendation_attach_retry_ = {};
    recommendation_sidecar_path_.clear();
    mapping_probe_session_.Clear();
    mapping_probe_logged_ = false;
    mapping_identity_logged_ = false;
    mapping_probe_request_sequence_ = 0;
    mapping_snapshot_request_sequence_ = 0;
    mapping_snapshot_task_sequence_ = 0;
    mapping_snapshot_attempt_ = 0;
    mapping_snapshot_generation_ = 0;
    mapping_snapshot_task_generation_ = 0;
    mapping_snapshot_request_active_ = false;
    mapping_snapshot_retry_deadline_ = {};
    mapping_snapshot_next_attempt_ = {};
    recommendation_detail_targets_.clear();
    ready_logged_.store(false);
    house_button_controller_.reset();
    house_move_probe_controller_.reset();
    furniture_move_probe_controller_.reset();
    recommendation_marker_controller_.reset();
    recommendation_marker_view_.reset();
    furniture_analysis_task_ = {};
    furniture_analysis_service_.reset();
    in_game_panel_controller_.reset();
    management_panel_view_.reset();
    config_runtime_.reset();
    organize_workflow_.reset();
    furniture_placement_gateway_.reset();
    runtime_move_gateway_.reset();
    house_button_view_.reset();
}

bool MewUiBridge::Available() const noexcept {
    return started_;
}

bool MewUiBridge::Ready() const noexcept {
    return started_ && MewUI_IsReady() != 0;
}

SceneContextService& MewUiBridge::SceneContext() noexcept {
    return scene_context_;
}

void __cdecl MewUiBridge::Tick(void* user_data) {
    auto* bridge = static_cast<MewUiBridge*>(user_data);
    if (bridge == nullptr || MewUI_IsReady() == 0) {
        return;
    }
    bridge->OnTick();
}

void MewUiBridge::OnTick() {
    const auto now = std::chrono::steady_clock::now();
    if (last_tick_time_.time_since_epoch().count() != 0 &&
        now - last_tick_time_ < std::chrono::milliseconds(8)) {
        return;
    }
    last_tick_time_ = now;
    if (config_runtime_) {
        const auto state = organize_workflow_
            ? organize_workflow_->State()
            : workflow::WorkflowState::Idle;
        const auto reload = config_runtime_->PollHotReload(state);
        if (reload.status == ConfigReloadStatus::Rejected) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "Config",
                "AC1304",
                "Runtime configuration hot-reload rejected; previous "
                "configuration remains active: " +
                    reload.message);
        }
    }
    if (!ready_logged_.exchange(true)) {
        Logger::Instance().Write(
            LogLevel::Info,
            Name(),
            "AC1202",
            "MewUI API hooks are ready on the game UI thread.");
    }

    std::array<AcMewSceneRecord, kSceneProbeCapacity> records{};
    const auto count = AcMewEnumerateScenes(records.data(), records.size());
    std::vector<RuntimeScene> scenes;
    scenes.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        scenes.push_back({
            records[index].scene_manager,
            records[index].scene_name,
            records[index].component_count,
            records[index].ready != 0
        });
    }

    const bool f8_pressed = (GetAsyncKeyState(VK_F8) & 1) != 0;
    const bool shift_pressed =
        (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    if (debug_probe_enabled_) {
        LogSceneSummary(scenes);
        if (f8_pressed && shift_pressed) {
            ExportSceneSummary(scenes);
        }
    }
    (void)scene_context_.Observe(ObserveScenes(scenes));

    const auto context = scene_context_.Current();
    UpdateHouseUiMode(context, scenes);
    if (furniture_analysis_task_.valid() &&
        furniture_analysis_task_.wait_for(std::chrono::milliseconds(0)) ==
            std::future_status::ready) {
        auto analysis = furniture_analysis_task_.get();
        const auto generation = furniture_analysis_task_generation_;
        if (generation != context.scene_generation || !furniture_mode_) {
            furniture_auto_run_active_ = false;
            ClearFurnitureLayoutPreview();
        } else {
            const bool english = config_runtime_ &&
                config_runtime_->Current().general.language == "en-US";
            std::vector<std::string> labels;
            if (analysis) {
                auto completed = std::move(analysis.value);
                const auto& plan = completed.layout_plan;
                const bool executable =
                    !plan.moves.empty() ||
                    !completed.attribute_upgrades.empty();
                const bool layout_blocked =
                    plan.current_state_blocked_room_count != 0U ||
                    plan.evacuation_blocked_room_count != 0U ||
                    plan.installation_blocked_room_count != 0U;
                const bool exhausted_any_room =
                    !plan.exhausted_room_ids.empty();
                if (!executable) {
                    auto runtime_furniture = CaptureRuntimeFurnitureState(
                        current_house_scene_manager_);
                    for (const auto& exhausted_room_id :
                         plan.exhausted_room_ids) {
                        if (runtime_furniture) {
                            LockFurnitureRoom(
                                furniture_locked_room_ids_,
                                furniture_locked_room_signatures_,
                                runtime_furniture.value,
                                exhausted_room_id);
                        }
                    }
                }
                if (!completed.attribute_upgrades.empty()) {
                    const auto gain = CompactAttributeGain(
                        completed.attribute_upgrade_gain);
                    labels = english
                        ? std::vector<std::string>{
                              "Rooms: " +
                                  std::to_string(completed.rooms.size()) +
                                  ", furniture: " +
                                  std::to_string(completed.furniture_count),
                              "Layout: " +
                                  std::to_string(plan.moves.size()) +
                                  " moves, " +
                                  std::to_string(plan.kept_furniture_count) +
                                  " already packed",
                              "Attribute upgrades: " +
                                  std::to_string(
                                      completed.attribute_upgrades.size()) +
                                  " (" + gain + ")",
                              "Warehouse scene pieces: " +
                                  std::to_string(
                                      completed.runtime_warehouse_piece_match_count) +
                                  "/" +
                                  std::to_string(
                                      completed.warehouse_furniture_count),
                              executable
                                  ? "Auto Place: upgrades and layout are ready"
                                  : "Auto Place: no safe operation is ready"}
                        : std::vector<std::string>{
                              "房间：" +
                                  std::to_string(completed.rooms.size()) +
                                  "，家具：" +
                                  std::to_string(completed.furniture_count),
                              "布局：需移动 " +
                                  std::to_string(plan.moves.size()) +
                                  " 件，已在紧凑位置 " +
                                  std::to_string(plan.kept_furniture_count) +
                                  " 件",
                              "属性升级候选：" +
                                  std::to_string(
                                      completed.attribute_upgrades.size()) +
                                  " 件（" + gain + "）",
                              "仓库场景对象：" +
                                  std::to_string(
                                      completed.runtime_warehouse_piece_match_count) +
                                  "/" +
                                  std::to_string(
                                      completed.warehouse_furniture_count),
                              executable
                                  ? "自动放置：属性替换与布局均可执行"
                                  : "自动放置：当前没有安全操作"};
                } else {
                    labels = english
                        ? std::vector<std::string>{
                          "Rooms: " + std::to_string(completed.rooms.size()) +
                              ", furniture: " +
                              std::to_string(completed.furniture_count),
                          "Layout: " + std::to_string(plan.moves.size()) +
                              " moves, " +
                              std::to_string(plan.kept_furniture_count) +
                              " already packed, " +
                              std::to_string(plan.deferred_furniture_count) +
                              " deferred",
                          "Skipped: warehouse " +
                              std::to_string(plan.warehouse_furniture_count) +
                              ", unsupported " +
                              std::to_string(plan.unsupported_furniture_count) +
                              ", no space " +
                              std::to_string(plan.no_space_furniture_count),
                          executable
                              ? "Auto Place: ready"
                              : (layout_blocked
                                  ? "Auto Place: layout is blocked; existing furniture was not treated as complete"
                              : (exhausted_any_room
                                  ? "Room complete; run analysis for the next room"
                                  : "Auto Place: no safe move needed"))}
                        : std::vector<std::string>{
                          "房间：" + std::to_string(completed.rooms.size()) +
                              "，家具：" +
                              std::to_string(completed.furniture_count),
                          "布局：需移动 " +
                              std::to_string(plan.moves.size()) +
                              " 件，已在紧凑位置 " +
                              std::to_string(plan.kept_furniture_count) +
                              " 件，本批延后 " +
                              std::to_string(plan.deferred_furniture_count) +
                              " 件",
                          "跳过：仓库 " +
                              std::to_string(plan.warehouse_furniture_count) +
                              "，不支持 " +
                              std::to_string(plan.unsupported_furniture_count) +
                              "，无空间 " +
                              std::to_string(plan.no_space_furniture_count),
                          executable
                              ? "自动放置：可以执行"
                              : (layout_blocked
                                  ? "自动放置：布局受阻，未把现状误判为已放满"
                              : (exhausted_any_room
                                  ? "本房间已完成，请再次分析下一个房间"
                                  : "自动放置：无需安全移动"))};
                }
                Logger::Instance().Write(
                    LogLevel::Info,
                    "FurnitureAnalysis",
                    "AC3901",
                    "Layout ready: generation=" +
                        std::to_string(generation) +
                        ", rooms=" +
                        std::to_string(plan.planned_room_count) +
                        ", moves=" +
                        std::to_string(plan.moves.size()) +
                        ", target=" +
                        SafeTechnicalName(plan.target_room_id) +
                        ", deferred=" +
                        std::to_string(plan.deferred_furniture_count) +
                        ", current_blocked=" +
                        std::to_string(
                            plan.current_state_blocked_room_count) +
                        ", evacuation_blocked=" +
                        std::to_string(
                            plan.evacuation_blocked_room_count) +
                        ", installation_blocked=" +
                        std::to_string(
                            plan.installation_blocked_room_count) +
                        ", tabu_filtered_moves=" +
                        std::to_string(plan.tabu_filtered_move_count) +
                        ", focus_room=" +
                        SafeTechnicalName(
                            furniture_focus_room_id_.value_or(
                                snapshot::RoomId{})) +
                        ", persistent_locked_rooms=" +
                        std::to_string(furniture_locked_room_ids_.size()) +
                        ", unsupported=" +
                        std::to_string(plan.unsupported_furniture_count) +
                        ", no_space=" +
                        std::to_string(plan.no_space_furniture_count) +
                        ", attribute_upgrades=" +
                        std::to_string(completed.attribute_upgrades.size()) +
                        ", binding=" + completed.binding_digest + ".");
                Logger::Instance().Write(
                    LogLevel::Info,
                    "FurnitureAnalysis",
                    "AC3911",
                    "Warehouse scene probe: saved=" +
                        std::to_string(completed.warehouse_furniture_count) +
                        ", scene_pieces=" +
                        std::to_string(completed.runtime_scene_piece_count) +
                        ", placed_pieces=" +
                        std::to_string(completed.runtime_placed_piece_count) +
                        ", grid_null_pieces=" +
                        std::to_string(completed.runtime_warehouse_piece_count) +
                        ", matched=" +
                        std::to_string(
                            completed.runtime_warehouse_piece_match_count) +
                        ".");
                for (std::size_t index = 0;
                     index < std::min<std::size_t>(
                         completed.attribute_upgrades.size(), 8U);
                     ++index) {
                    const auto& upgrade = completed.attribute_upgrades[index];
                    Logger::Instance().Write(
                        LogLevel::Info,
                        "FurnitureAnalysis",
                        "AC3910",
                        "Attribute upgrade " + std::to_string(index + 1U) +
                            ": room=" +
                            SafeTechnicalName(upgrade.target_room_id) +
                            " placed=" +
                            SafeTechnicalName(upgrade.placed_item_id) +
                            " key=" +
                            std::to_string(upgrade.placed_stable_key) +
                            " -> warehouse=" +
                            SafeTechnicalName(upgrade.warehouse_item_id) +
                            " key=" +
                            std::to_string(upgrade.warehouse_stable_key) +
                            " target=(" +
                            std::to_string(upgrade.target_x) + "," +
                            std::to_string(upgrade.target_y) + ") gain=" +
                            CompactAttributeGain(upgrade.gain) + ".");
                }
                if (!plan.target_room_id.empty()) {
                    furniture_focus_room_id_ = plan.target_room_id;
                }
                furniture_analysis_preview_ = std::move(completed);
                furniture_auto_run_preview_fresh_ =
                    furniture_auto_run_active_;
                if (house_button_controller_) {
                    house_button_controller_->SetFurnitureActionAvailable(
                        executable);
                }
                if (furniture_auto_run_active_) {
                    const auto& preview_plan =
                        furniture_analysis_preview_->layout_plan;
                    if (executable) {
                        if (std::chrono::steady_clock::now() >=
                            furniture_auto_run_next_transaction_) {
                            StartFurnitureAutoPlacement(generation);
                        }
                    } else if (layout_blocked) {
                        furniture_auto_run_active_ = false;
                        if (house_button_controller_) {
                            house_button_controller_->SetState(
                                OrganizeButtonState::Failed,
                                "furniture planning is blocked before reaching a safe final layout");
                        }
                        Logger::Instance().Write(
                            LogLevel::Warn,
                            "FurniturePlacement",
                            "AC3929",
                            "Continuous Auto Place stopped because layout planning is blocked, not because a safe fixpoint was reached: current_blocked=" +
                                std::to_string(
                                    preview_plan.current_state_blocked_room_count) +
                                ", evacuation_blocked=" +
                                std::to_string(
                                    preview_plan.evacuation_blocked_room_count) +
                                ", installation_blocked=" +
                                std::to_string(
                                    preview_plan.installation_blocked_room_count) +
                                ", deferred=" +
                                std::to_string(
                                    preview_plan.deferred_furniture_count) +
                                ", state_tabu_edges=" +
                                std::to_string(
                                    furniture_layout_move_tabu_.size()) +
                                ".");
                    } else if (exhausted_any_room) {
                        ClearFurnitureLayoutPreview();
                        StartFurnitureAnalysis(generation);
                    } else {
                        furniture_auto_run_active_ = false;
                        if (house_button_controller_) {
                            house_button_controller_->SetState(
                                OrganizeButtonState::Completed,
                                "furniture organizer reached the current-scene safe fixpoint");
                        }
                        Logger::Instance().Write(
                            LogLevel::Info,
                            "FurniturePlacement",
                            "AC3922",
                            "Continuous Auto Place reached the current-scene safe fixpoint: upgrades=" +
                                std::to_string(furniture_auto_run_upgraded_) +
                                ", layout_moves=" +
                                std::to_string(furniture_auto_run_moved_) +
                                ", quarantined_keys=" +
                                std::to_string(furniture_retired_keys_.size()) +
                                ", blocked_rooms=" +
                                std::to_string(
                                    furniture_auto_run_blocked_rooms_) +
                                ", persistent_locked_rooms=" +
                                std::to_string(
                                    furniture_locked_room_ids_.size()) +
                                ", tabu_moves=" +
                                std::to_string(
                                    furniture_layout_move_tabu_.size()) +
                                ", focus_room=" +
                                SafeTechnicalName(
                                    furniture_focus_room_id_.value_or(
                                        snapshot::RoomId{})) +
                                ".");
                    }
                }
            } else {
                furniture_auto_run_active_ = false;
                ClearFurnitureLayoutPreview();
                labels = english
                    ? std::vector<std::string>{
                          "Analysis unavailable", "See AutoCattery log",
                          "No furniture was moved",
                          "Auto Place remains disabled"}
                    : std::vector<std::string>{
                          "分析不可用", "请查看 AutoCattery 日志",
                          "没有移动任何家具", "自动放置保持禁用"};
                Logger::Instance().Write(
                    LogLevel::Warn,
                    "FurnitureAnalysis",
                    "AC3205",
                    "Read-only analysis failed: " + analysis.message);
            }
            if (recommendation_marker_controller_) {
                (void)recommendation_marker_controller_->ShowFurnitureAnalysis(
                    generation, labels);
            }
        }
    }
    if (house_button_controller_) {
        house_button_controller_->Poll();
    }
    if (recommendation_marker_controller_) {
        recommendation_marker_controller_->Poll();
    }
    const auto house_scene = std::find_if(
        scenes.begin(),
        scenes.end(),
        [&context](const RuntimeScene& candidate) {
            return candidate.ready &&
                   candidate.name == context.scene_name;
        });
    if (in_game_panel_controller_) {
        in_game_panel_controller_->Poll(
            context,
            house_scene == scenes.end() ? nullptr : house_scene->manager,
            (GetAsyncKeyState(VK_F10) & 1) != 0,
            (GetAsyncKeyState(VK_ESCAPE) & 1) != 0);
    }
    const bool writable_house =
        context.kind == UiContextKind::House &&
        context.input_enabled &&
        !context.save_in_progress &&
        house_scene != scenes.end();
    if (runtime_move_gateway_) {
        runtime_move_gateway_->SetHouseScene(
            writable_house ? house_scene->manager : nullptr);
        current_house_scene_manager_ =
            writable_house ? house_scene->manager : nullptr;
        if (writable_house &&
            runtime_snapshot_adapter_ &&
            runtime_snapshot_context_generation_ !=
                context.scene_generation) {
            RefreshRuntimeSnapshotContext();
            runtime_snapshot_context_generation_ =
                context.scene_generation;
        }
    }
    if (furniture_placement_gateway_) {
        furniture_placement_gateway_->SetHouseScene(
            writable_house ? house_scene->manager : nullptr);
    }
    if (furniture_auto_run_active_ &&
        !furniture_execution_active_ &&
        !furniture_analysis_preview_ &&
        !furniture_analysis_task_.valid() &&
        std::chrono::steady_clock::now() >=
            furniture_auto_run_next_transaction_) {
        StartFurnitureAnalysis(context.scene_generation);
    }
    if (furniture_auto_run_active_ &&
        !furniture_execution_active_ &&
        furniture_analysis_preview_ &&
        !furniture_analysis_task_.valid() &&
        std::chrono::steady_clock::now() >=
            furniture_auto_run_next_transaction_) {
        StartFurnitureAutoPlacement(context.scene_generation);
    }
    PollFurnitureAutoPlacement(context);
#ifdef _DEBUG
    if (debug_probe_enabled_ && f8_pressed && !shift_pressed &&
        !furniture_execution_active_) {
        RunFurnitureNativeMoveTest(context);
    }
#endif
    if (debug_probe_enabled_ && house_move_probe_controller_) {
        const auto event = house_move_probe_controller_->Poll(
            context,
            house_scene == scenes.end() ? nullptr : house_scene->manager,
            (GetAsyncKeyState(VK_F9) & 1) != 0,
            (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
        if (event.kind != HouseMoveProbeEventKind::None) {
            Logger::Instance().Write(
                event.kind == HouseMoveProbeEventKind::Rejected
                    ? LogLevel::Warn
                    : LogLevel::Info,
                "HouseMoveProbe",
                "AC14200",
                event.message);
        }
    }
    if (furniture_move_probe_controller_) {
        const auto event = furniture_move_probe_controller_->Poll(
            context,
            house_scene == scenes.end() ? nullptr : house_scene->manager,
            furniture_mode_,
            furniture_mode_component_,
            (GetAsyncKeyState(VK_F7) & 1) != 0);
        if (event.kind != FurnitureMoveProbeEventKind::None) {
            Logger::Instance().Write(
                event.kind == FurnitureMoveProbeEventKind::Rejected ||
                        event.kind == FurnitureMoveProbeEventKind::Cancelled
                    ? LogLevel::Warn
                    : LogLevel::Info,
                "FurnitureMoveProbe",
                "AC3700",
                event.message);
            if (furniture_mode_ && recommendation_marker_controller_) {
                const bool english = config_runtime_ &&
                    config_runtime_->Current().general.language == "en-US";
                std::vector<std::string> labels;
                if (event.kind ==
                    FurnitureMoveProbeEventKind::BeforeCaptured) {
                    labels = english
                        ? std::vector<std::string>{
                              "Furniture probe: before captured",
                              "Take one item from the warehouse drawer",
                              "Place it in a room, then press F7 again",
                              "The MOD will not move it automatically"}
                        : std::vector<std::string>{
                              "家具探针：已记录移动前",
                              "请从仓库抽屉取出一件家具",
                              "把它放进任一房间后再次按 F7",
                              "MOD 不会自动移动家具"};
                } else if (event.kind ==
                    FurnitureMoveProbeEventKind::ReportWritten) {
                    labels = english
                        ? std::vector<std::string>{
                              "Furniture probe: capture complete",
                              event.report_filename,
                              "Save and fully exit the game",
                              "Codex will inspect the report"}
                        : std::vector<std::string>{
                              "家具探针：采集完成",
                              event.report_filename,
                              "请保存并完全退出游戏",
                              "Codex 将读取报告继续接原生移动"};
                } else {
                    labels = english
                        ? std::vector<std::string>{
                              "Furniture probe unavailable",
                              "Leave and re-enter furniture mode",
                              "Press F7 to retry",
                              "No furniture was moved by the MOD"}
                        : std::vector<std::string>{
                              "家具探针未完成",
                              "请退出并重新进入家具界面",
                              "按 F7 重试",
                              "MOD 没有移动任何家具"};
                }
                (void)recommendation_marker_controller_->ShowFurnitureAnalysis(
                    context.scene_generation,
                    labels);
            }
        }
    }
    ObserveMappingProbe(context, scenes);
    ObserveHouseCatIdentity(context, scenes);
    const auto scene_ready =
        [&scenes](std::string_view name) {
            return std::any_of(
                scenes.begin(),
                scenes.end(),
                [name](const RuntimeScene& scene) {
                    return scene.ready && scene.name == name;
                });
        };
    const bool house_ready =
        context.kind == UiContextKind::House &&
        context.input_enabled &&
        !context.save_in_progress;
    const auto active_config = config_runtime_
        ? config_runtime_->Current()
        : Config{};
    const bool mod_ui_enabled = active_config.general.mod_enabled;
    const bool panel_open = in_game_panel_controller_ &&
        in_game_panel_controller_->IsOpen();
    const bool house_button_enabled =
        mod_ui_enabled &&
        (furniture_mode_ || active_config.ui.house_button_enabled);
    const bool recommendation_button_enabled =
        mod_ui_enabled &&
        (furniture_mode_ || active_config.ui.embark_button_enabled);
    const bool interstitial_ready = scene_ready("Interstitial");
    const bool expedition_ready =
        scene_ready("Map") || scene_ready("Battle");
    const bool save_selection_ready = scene_ready("SaveSelectionScreen");
    if (house_button_controller_) {
        house_button_controller_->SetSuppressed(panel_open);
    }
    if (recommendation_marker_controller_) {
        recommendation_marker_controller_->SetSuppressed(panel_open);
    }
    recommendation_marker_controller_->ObserveRuntime(
        house_ready,
        interstitial_ready,
        expedition_ready,
        save_selection_ready);

    if (!house_button_enabled && house_button_controller_ &&
        house_button_controller_->IsAttached()) {
        house_button_controller_->Detach();
    }
    if (!recommendation_button_enabled &&
        recommendation_marker_controller_ &&
        recommendation_marker_controller_->IsAttached()) {
        recommendation_marker_controller_->Detach();
    }

    if (house_button_enabled &&
        !panel_open &&
        context.kind == UiContextKind::House &&
        context.input_enabled &&
        !context.save_in_progress &&
        house_button_controller_ &&
        !house_button_controller_->IsAttached() &&
        (next_house_attach_retry_.time_since_epoch().count() == 0 ||
         now >= next_house_attach_retry_)) {
        const auto attached = house_button_controller_->Attach(context);
        if (!attached) {
            if (attached.message != last_house_attach_error_) {
                last_house_attach_error_ = attached.message;
                Logger::Instance().Write(
                    LogLevel::Warn,
                    "HouseButton",
                    "AC3103",
                    "House button attach deferred: " + attached.message);
            }
            next_house_attach_retry_ =
                now + std::chrono::milliseconds(500);
        } else {
            next_house_attach_retry_ = {};
            last_house_attach_error_.clear();
        }
    }

    if (recommendation_button_enabled &&
        !panel_open &&
        house_ready &&
        !recommendation_marker_controller_->IsAttached() &&
        (next_recommendation_attach_retry_.time_since_epoch().count() == 0 ||
         now >= next_recommendation_attach_retry_)) {
        const auto attached =
            recommendation_marker_controller_->Attach(context);
        if (!attached) {
            if (attached.message !=
                last_recommendation_attach_error_) {
                last_recommendation_attach_error_ = attached.message;
                Logger::Instance().Write(
                    LogLevel::Warn,
                    "RecommendationMarker",
                    "AC4105",
                    "Recommendation button attach deferred: " +
                        attached.message);
            }
            next_recommendation_attach_retry_ =
                now + std::chrono::milliseconds(500);
        } else {
            next_recommendation_attach_retry_ = {};
            last_recommendation_attach_error_.clear();
        }
    }
}

void MewUiBridge::UpdateHouseUiMode(
    const UiContextSnapshot& context,
    const std::vector<RuntimeScene>& scenes) {
    const auto house_scene = std::find_if(
        scenes.begin(),
        scenes.end(),
        [&context](const RuntimeScene& candidate) {
            return candidate.ready &&
                   context.kind == UiContextKind::House &&
                   candidate.name == context.scene_name;
        });
    if (house_scene == scenes.end()) {
        furniture_mode_scene_manager_ = nullptr;
        furniture_mode_component_count_ = 0;
        furniture_mode_component_ = nullptr;
        return;
    }

    if (furniture_mode_scene_manager_ != house_scene->manager ||
        furniture_mode_component_count_ != house_scene->component_count) {
        furniture_mode_scene_manager_ = house_scene->manager;
        furniture_mode_component_count_ = house_scene->component_count;
        furniture_mode_component_ = AcMewFindComponentByType(
            house_scene->manager,
            kFurnitureBuildingComponent);
    }

    const bool detected =
        AcMewFurnitureBuildingUiIsActive(furniture_mode_component_) != 0;
    if (detected == furniture_mode_) {
        return;
    }

    furniture_mode_ = detected;
    if (furniture_mode_) {
        furniture_attribute_upgrade_committed_in_mode_ = false;
    } else {
        furniture_auto_run_active_ = false;
    }
    ClearFurnitureLayoutPreview();
    recommendation_detail_targets_.clear();
    mapping_snapshot_request_active_ = false;
    mapping_snapshot_retry_deadline_ = {};
    mapping_snapshot_next_attempt_ = {};
    if (house_button_controller_) {
        house_button_controller_->SetFurnitureMode(furniture_mode_);
    }
    if (recommendation_marker_controller_) {
        recommendation_marker_controller_->SetFurnitureMode(
            furniture_mode_);
    }
    Logger::Instance().Write(
        LogLevel::Info,
        "HouseUiMode",
        "AC3210",
        furniture_mode_
            ? "Furniture placement state opened; controls switched to Auto Place and Start Analysis."
            : "Furniture placement state closed; normal Auto-Organize and combat recommendation controls restored.");
}

void MewUiBridge::RefreshRuntimeSnapshotContext() {
    if (!runtime_snapshot_adapter_ ||
        !current_house_scene_manager_) {
        return;
    }
    auto runtime_furniture = CaptureRuntimeFurnitureState(
        current_house_scene_manager_);
    if (!runtime_furniture) {
        runtime_snapshot_adapter_->SetRuntimeContext(0U, 0U);
        Logger::Instance().Write(
            LogLevel::Warn,
            "RuntimeSaveSelection",
            "AC14320",
            "Live House furniture refresh failed closed: " +
                runtime_furniture.message);
        return;
    }
    std::vector<snapshot::RoomId> invalidated_room_ids;
    ReconcileLockedFurnitureRooms(
        furniture_locked_room_ids_,
        furniture_locked_room_signatures_,
        runtime_furniture.value,
        &invalidated_room_ids);
    if (!invalidated_room_ids.empty()) {
        for (const auto& room_id : invalidated_room_ids) {
            if (furniture_focus_room_id_ == room_id) {
                furniture_focus_room_id_.reset();
            }
        }
        Logger::Instance().Write(
            LogLevel::Info,
            "FurnitureAnalysis",
            "AC3927",
            "Invalidated stale completed-room locks after live furniture changed: rooms=" +
                std::to_string(invalidated_room_ids.size()) +
                ", remaining=" +
                std::to_string(furniture_locked_room_ids_.size()) + ".");
    }
    const auto runtime_furniture_count =
        runtime_furniture.value.placements.size();
    const auto runtime_scene_piece_count =
        runtime_furniture.value.scene_piece_count;
    const auto runtime_warehouse_piece_count =
        runtime_furniture.value.warehouse_pieces.size();
    if (!runtime_move_available_) {
        std::array<void*, 16> native_rooms{};
        const auto native_room_count =
            AcMewEnumerateNativeHouseRooms(
                current_house_scene_manager_,
                native_rooms.data(),
                native_rooms.size());
        const auto house_cat_count =
            AcMewCountHouseCats(current_house_scene_manager_);
        runtime_snapshot_adapter_->SetRuntimeContext(
            house_cat_count,
            native_room_count > 2U ? native_room_count - 2U : 0U,
            std::move(runtime_furniture.value));
        return;
    }
    auto runtime = CaptureRuntimeHouseState(
        current_house_scene_manager_);
    if (!runtime) {
        runtime_snapshot_adapter_->SetRuntimeContext(0U, 0U);
        Logger::Instance().Write(
            LogLevel::Warn,
            "RuntimeSaveSelection",
            "AC14318",
            "Live House room refresh failed closed: " + runtime.message);
        return;
    }
    const auto house_cat_count = runtime.value.cats.size();
    const auto available_room_count =
        runtime.value.available_room_count;
    const auto native_room_count = runtime.value.rooms.size();
    runtime_snapshot_adapter_->SetRuntimeHouseState(
        std::move(runtime.value),
        std::move(runtime_furniture.value));
    Logger::Instance().Write(
        LogLevel::Info,
        "RuntimeSaveSelection",
        "AC14315",
        "House runtime context refreshed: cats=" +
            std::to_string(house_cat_count) +
            ", native room components=" +
            std::to_string(native_room_count) +
            ", available rooms=" +
            std::to_string(available_room_count) +
            ", furniture=" +
            std::to_string(runtime_furniture_count) +
            ", scene pieces=" +
            std::to_string(runtime_scene_piece_count) +
            ", warehouse pieces=" +
            std::to_string(runtime_warehouse_piece_count));
}

void MewUiBridge::ClearFurnitureLayoutPreview() {
    furniture_analysis_preview_.reset();
    furniture_execution_active_ = false;
    furniture_execution_generation_ = 0;
    furniture_execution_upgrade_index_ = 0;
    furniture_execution_index_ = 0;
    furniture_execution_upgraded_ = 0;
    furniture_execution_moved_ = 0;
    furniture_execution_committed_upgrade_indices_.clear();
    furniture_execution_committed_move_indices_.clear();
    if (house_button_controller_) {
        house_button_controller_->SetFurnitureActionAvailable(false);
    }
}

void MewUiBridge::StartFurnitureAnalysis(std::uint64_t generation) {
    if (furniture_execution_active_ ||
        furniture_faulted_generation_ == generation) {
        return;
    }
    if (furniture_layout_session_generation_ != generation) {
        furniture_layout_session_generation_ = generation;
        furniture_locked_room_ids_.clear();
        furniture_locked_room_signatures_.clear();
        furniture_room_purposes_.clear();
        furniture_layout_move_tabu_.clear();
        furniture_layout_attempted_state_edges_.clear();
        furniture_focus_room_id_.reset();
    }
    if (furniture_retirement_generation_ != generation) {
        furniture_retirement_generation_ = generation;
        furniture_retired_keys_.clear();
        if (furniture_placement_gateway_) {
            furniture_placement_gateway_->SetBlockedWarehouseKeys({});
        }
    }
    // Completed rooms and retired stable keys remain scoped to the current
    // House scene. Re-entering furniture mode does not make a queued-delete
    // stable key safe to create again.
    ClearFurnitureLayoutPreview();
    RefreshRuntimeSnapshotContext();
    if (!furniture_analysis_service_ ||
        (furniture_analysis_task_.valid() &&
         furniture_analysis_task_.wait_for(std::chrono::milliseconds(0)) !=
             std::future_status::ready)) {
        return;
    }
    const auto locked_room_ids = furniture_locked_room_ids_;
    const auto blocked_keys = furniture_retired_keys_;
    const bool allow_attribute_upgrades =
        !furniture_attribute_upgrade_committed_in_mode_;
    if (furniture_room_purposes_.empty() && organize_workflow_) {
        const auto purposes =
            organize_workflow_->BuildFurnitureRoomPurposes(generation);
        if (purposes) {
            furniture_room_purposes_ = purposes.value.room_purposes;
        } else {
            Logger::Instance().Write(
                LogLevel::Warn,
                "FurnitureAnalysis",
                "AC3925",
                "Room-purpose analysis unavailable; furniture scoring will use conservative general-room objectives: " +
                    purposes.message);
        }
    }
    const auto room_purposes = furniture_room_purposes_;
    const auto forbidden_layout_moves = furniture_layout_move_tabu_;
    const auto preferred_focus_room_id =
        furniture_focus_room_id_.value_or(snapshot::RoomId{});
    furniture_analysis_task_generation_ = generation;
    furniture_analysis_task_ = std::async(
        std::launch::async,
        [this, generation, locked_room_ids, blocked_keys,
         allow_attribute_upgrades, room_purposes, forbidden_layout_moves,
         preferred_focus_room_id] {
            return furniture_analysis_service_->Analyze(
                generation,
                locked_room_ids,
                blocked_keys,
                allow_attribute_upgrades,
                room_purposes,
                forbidden_layout_moves,
                preferred_focus_room_id);
        });
}

void MewUiBridge::StartFurnitureAutoPlacement(
    std::uint64_t generation) {
    const auto context = scene_context_.Current();
    const bool valid =
        furniture_mode_ &&
        context.kind == UiContextKind::House &&
        context.input_enabled &&
        !context.save_in_progress &&
        context.scene_generation == generation &&
        furniture_placement_gateway_ &&
        furniture_analysis_service_ &&
        furniture_analysis_preview_.has_value() &&
        furniture_analysis_preview_->scene_generation == generation &&
        (!furniture_analysis_preview_->layout_plan.moves.empty() ||
         !furniture_analysis_preview_->attribute_upgrades.empty());
    if (!valid) {
        ClearFurnitureLayoutPreview();
        if (house_button_controller_) {
            house_button_controller_->SetState(
                OrganizeButtonState::Failed,
                "the furniture layout preview is unavailable or stale");
        }
        Logger::Instance().Write(
            LogLevel::Warn,
            "FurniturePlacement",
            "AC3902",
            "Auto Place rejected because the layout preview or House generation is stale.");
        return;
    }

    if (!furniture_auto_run_preview_fresh_) {
        RefreshRuntimeSnapshotContext();
        const auto blocked_keys = furniture_retired_keys_;
        const auto forbidden_layout_moves = furniture_layout_move_tabu_;
        const auto preferred_focus_room_id =
            furniture_focus_room_id_.value_or(snapshot::RoomId{});
        const auto refreshed = furniture_analysis_service_->Analyze(
            generation,
            furniture_locked_room_ids_,
            blocked_keys,
            !furniture_attribute_upgrade_committed_in_mode_,
            furniture_room_purposes_,
            forbidden_layout_moves,
            preferred_focus_room_id);
        if (!refreshed ||
            refreshed.value.binding_digest !=
                furniture_analysis_preview_->binding_digest) {
            ClearFurnitureLayoutPreview();
            if (house_button_controller_) {
                house_button_controller_->SetState(
                    OrganizeButtonState::Failed,
                    "the furniture layout changed after analysis");
            }
            Logger::Instance().Write(
                LogLevel::Warn,
                "FurniturePlacement",
                "AC3906",
                "Auto Place rejected because the sealed whole-house furniture snapshot changed after analysis.");
            return;
        }
    }

    const bool continuing_auto_run = furniture_auto_run_active_;
    if (!continuing_auto_run) {
        furniture_auto_run_upgraded_ = 0;
        furniture_auto_run_moved_ = 0;
        furniture_auto_run_blocked_rooms_ = 0;
        furniture_auto_run_transaction_count_ = 0;
        furniture_auto_run_next_transaction_ = {};
    } else if (std::chrono::steady_clock::now() <
               furniture_auto_run_next_transaction_) {
        return;
    }

    if (furniture_analysis_preview_->attribute_upgrades.empty() &&
        !furniture_analysis_preview_->layout_plan.moves.empty()) {
        const auto blocked_room =
            furniture_analysis_preview_->layout_plan.target_room_id;
        const auto repeated =
            furniture_analysis_preview_->layout_plan.moves.front();
        const auto edge_status =
            furniture_planning::RecordFurnitureLayoutStateEdge(
                furniture_layout_attempted_state_edges_,
                furniture_analysis_preview_->binding_digest,
                repeated);
        if (edge_status == furniture_planning::
                FurnitureLayoutStateEdgeRecordStatus::CapacityReached) {
            furniture_auto_run_active_ = false;
            ClearFurnitureLayoutPreview();
            if (house_button_controller_) {
                house_button_controller_->SetState(
                    OrganizeButtonState::Failed,
                    "layout search history reached its safe limit");
            }
            Logger::Instance().Write(
                LogLevel::Warn,
                "FurniturePlacement",
                "AC3928",
                "Continuous Auto Place paused because the current House scene reached the bounded layout state-edge history limit: attempted_edges=" +
                    std::to_string(
                        furniture_layout_attempted_state_edges_.size()) +
                    ". Re-enter the House scene before retrying.");
            return;
        }
        if (edge_status == furniture_planning::
                FurnitureLayoutStateEdgeRecordStatus::Duplicate) {
            const furniture_planning::FurnitureLayoutStateEdge tabu_edge{
                furniture_analysis_preview_->binding_digest,
                repeated};
            if (std::ranges::find(
                    furniture_layout_move_tabu_, tabu_edge) ==
                furniture_layout_move_tabu_.end()) {
                furniture_layout_move_tabu_.push_back(tabu_edge);
            }
            furniture_focus_room_id_ = blocked_room;
            ++furniture_auto_run_blocked_rooms_;
            Logger::Instance().Write(
                LogLevel::Warn,
                "FurniturePlacement",
                "AC3924",
                "Continuous Auto Place rejected a previously attempted move from a repeated whole-house layout state and will keep searching the focused room: binding=" +
                    furniture_analysis_preview_->binding_digest + " room=" +
                    SafeTechnicalName(blocked_room) + " item=" +
                    SafeTechnicalName(repeated.item_id) + " key=" +
                    std::to_string(repeated.stable_key) + " from=(" +
                    std::to_string(repeated.from_x) + "," +
                    std::to_string(repeated.from_y) + ") target=(" +
                    std::to_string(repeated.target_x) + "," +
                    std::to_string(repeated.target_y) + ") tabu_moves=" +
                    std::to_string(furniture_layout_move_tabu_.size()) +
                    " attempted_edges=" +
                    std::to_string(
                        furniture_layout_attempted_state_edges_.size()) +
                    ".");
            ClearFurnitureLayoutPreview();
            StartFurnitureAnalysis(generation);
            return;
        }
    }

    furniture_execution_active_ = true;
    furniture_auto_run_active_ = true;
    furniture_auto_run_preview_fresh_ = false;
    furniture_execution_generation_ = generation;
    furniture_execution_upgrade_index_ = 0;
    furniture_execution_index_ = 0;
    furniture_execution_upgraded_ = 0;
    furniture_execution_moved_ = 0;
    furniture_execution_committed_upgrade_indices_.clear();
    furniture_execution_committed_move_indices_.clear();
    if (recommendation_marker_controller_) {
        const bool english = config_runtime_ &&
            config_runtime_->Current().general.language == "en-US";
        const auto upgrades =
            furniture_analysis_preview_->attribute_upgrades.size();
        const auto moves =
            furniture_analysis_preview_->layout_plan.moves.size();
        const auto total = upgrades + moves;
        const auto labels = english
            ? std::vector<std::string>{
                  "Auto Place started",
                  "Planned operations: " + std::to_string(total),
                  "Upgrades: " + std::to_string(upgrades) +
                      ", layout moves: " + std::to_string(moves),
                  "Moving one item per UI tick",
                  "The process stops and rolls back on the first rejection"}
            : std::vector<std::string>{
                  "自动放置已开始",
                  "计划操作：" + std::to_string(total) + " 项",
                  "属性替换：" + std::to_string(upgrades) +
                      "，布局移动：" + std::to_string(moves),
                  "每个 UI tick 移动一件",
                  "遇到首个拒绝项即停止并回滚"};
        (void)recommendation_marker_controller_->ShowFurnitureAnalysis(
            generation, labels);
    }
}

void MewUiBridge::PollFurnitureAutoPlacement(
    const UiContextSnapshot& context) {
    if (!furniture_execution_active_) {
        return;
    }
    if (std::chrono::steady_clock::now() <
        furniture_auto_run_next_transaction_) {
        return;
    }

    const auto finish = [this, &context] {
        if (!furniture_analysis_preview_) {
            return;
        }
        const auto& plan = furniture_analysis_preview_->layout_plan;
        const auto upgrades = furniture_execution_upgraded_;
        const auto moved = furniture_execution_moved_;
        const auto kept = plan.kept_furniture_count;
        furniture_auto_run_upgraded_ += upgrades;
        furniture_auto_run_moved_ += moved;
        furniture_auto_run_transaction_count_ += upgrades + moved;
        furniture_auto_run_next_transaction_ =
            std::chrono::steady_clock::now() +
            kFurnitureTransactionSettleDelay;
        auto completed_room_ids = plan.exhausted_room_ids;
        if (!plan.target_room_id.empty() && !plan.moves.empty() &&
            moved == plan.moves.size()) {
            completed_room_ids.push_back(plan.target_room_id);
            furniture_auto_run_active_ = false;
        }
        std::ranges::sort(completed_room_ids);
        const auto completed_unique = std::ranges::unique(completed_room_ids);
        completed_room_ids.erase(
            completed_unique.begin(), completed_unique.end());
        for (const auto& exhausted_room_id : completed_room_ids) {
            if (std::ranges::find(
                    furniture_locked_room_ids_, exhausted_room_id) ==
                furniture_locked_room_ids_.end()) {
                auto runtime_furniture = CaptureRuntimeFurnitureState(
                    current_house_scene_manager_);
                if (runtime_furniture) {
                    LockFurnitureRoom(
                        furniture_locked_room_ids_,
                        furniture_locked_room_signatures_,
                        runtime_furniture.value,
                        exhausted_room_id);
                }
            }
            if (furniture_focus_room_id_ == exhausted_room_id) {
                furniture_focus_room_id_.reset();
            }
        }
        Logger::Instance().Write(
            LogLevel::Info,
            "FurniturePlacement",
            "AC3904",
            "Auto Place completed: upgrades=" +
                std::to_string(upgrades) + "/" +
                std::to_string(
                    furniture_analysis_preview_->attribute_upgrades.size()) +
                ", layout_moves=" + std::to_string(moved) + "/" +
                std::to_string(plan.moves.size()) +
                ", already_packed=" + std::to_string(kept) + ".");
        if (recommendation_marker_controller_ && !furniture_auto_run_active_) {
            const bool english = config_runtime_ &&
                config_runtime_->Current().general.language == "en-US";
            const auto labels = english
                ? std::vector<std::string>{
                      "Auto Place complete",
                      "Warehouse upgrades: " + std::to_string(upgrades),
                      "Committed layout moves: " + std::to_string(moved),
                      "Already packed: " + std::to_string(kept),
                      "Save, exit, and re-enter to confirm the result"}
                : std::vector<std::string>{
                      "自动放置完成",
                      "仓库属性替换：" + std::to_string(upgrades) + " 件",
                      "已提交布局移动：" + std::to_string(moved) + " 件",
                      "原本已紧凑：" + std::to_string(kept) + " 件",
                      "请保存、退出并重进确认结果"};
            (void)recommendation_marker_controller_->ShowFurnitureAnalysis(
                context.scene_generation, labels);
        }
        const bool continue_same_room = furniture_auto_run_active_;
        ClearFurnitureLayoutPreview();
        if (continue_same_room) {
            auto blocked =
                furniture_placement_gateway_->BlockedWarehouseKeys();
            blocked.insert(
                blocked.end(),
                furniture_retired_keys_.begin(),
                furniture_retired_keys_.end());
            std::ranges::sort(blocked);
            const auto unique = std::ranges::unique(blocked);
            blocked.erase(unique.begin(), unique.end());
            furniture_retired_keys_ = std::move(blocked);
            Logger::Instance().Write(
                LogLevel::Info,
                "FurniturePlacement",
                "AC3921",
                "Continuous Auto Place will re-analyze after the committed transaction; quarantined_keys=" +
                    std::to_string(furniture_retired_keys_.size()) + ".");
        }
        if (house_button_controller_) {
            house_button_controller_->SetState(
                continue_same_room
                    ? OrganizeButtonState::Running
                    : OrganizeButtonState::Completed,
                continue_same_room
                    ? "refreshing the current room after its warehouse transaction"
                    : "the current room reached its final furniture layout");
        }
    };

    const auto fail = [this, &context](std::string reason) {
        const auto completed =
            furniture_execution_upgraded_ + furniture_execution_moved_;
        const auto total_upgrades = furniture_analysis_preview_
            ? furniture_analysis_preview_->attribute_upgrades.size()
            : 0U;
        const auto total_moves = furniture_analysis_preview_
            ? furniture_analysis_preview_->layout_plan.moves.size()
            : 0U;
        const auto completed_steps =
            furniture_execution_upgrade_index_ + furniture_execution_index_;
        const auto total = total_upgrades + total_moves;
        const auto remaining = total > completed_steps
            ? total - completed_steps
            : 0U;
        std::size_t rollback_completed{};
        const auto rollback_total =
            furniture_execution_committed_move_indices_.size();
        std::string rollback_detail = "rollback was not safe in the current scene";
        const bool rollback_safe =
            furniture_analysis_preview_ &&
            furniture_placement_gateway_ &&
            furniture_mode_ &&
            context.kind == UiContextKind::House &&
            context.input_enabled &&
            !context.save_in_progress &&
            context.scene_generation == furniture_execution_generation_;
        if (rollback_safe) {
            rollback_detail.clear();
            const auto& moves =
                furniture_analysis_preview_->layout_plan.moves;
            for (auto iterator =
                     furniture_execution_committed_move_indices_.rbegin();
                 iterator !=
                     furniture_execution_committed_move_indices_.rend();
                 ++iterator) {
                if (*iterator >= moves.size()) {
                    rollback_detail =
                        "rollback cursor exceeded the sealed plan";
                    break;
                }
                const auto& move = moves[*iterator];
                const FurniturePlacementLocator locator{
                    move.item_id, move.stable_key};
                if (furniture_planning::IsWarehouseLayoutMove(move)) {
                    const auto stored =
                        furniture_placement_gateway_->StorePlacedFurniture(
                            locator);
                    if (stored.status !=
                        FurnitureWarehousePlacementStatus::Stored) {
                        rollback_detail =
                            "native rollback could not return warehouse furniture " +
                            move.item_id + ": " + stored.message;
                        break;
                    }
                    ++rollback_completed;
                    continue;
                }
                const auto location =
                    furniture_placement_gateway_->Locate(locator);
                if (location.status !=
                        FurniturePlacementLookupStatus::Found ||
                    location.room != move.target_room_id ||
                    location.saved_x != move.target_x ||
                    location.saved_y != move.target_y) {
                    rollback_detail =
                        "rollback binding no longer matched the committed target";
                    break;
                }
                const auto restored = furniture_placement_gateway_->Move({
                    locator,
                    move.from_room_id,
                    move.from_x,
                    move.from_y,
                    true});
                if (restored.status != FurniturePlacementMoveStatus::Moved &&
                    restored.status !=
                        FurniturePlacementMoveStatus::AlreadyPlaced) {
                    rollback_detail =
                        "native rollback rejected " + move.item_id +
                        ": " + restored.message;
                    break;
                }
                ++rollback_completed;
            }
            if (rollback_completed == rollback_total) {
                rollback_detail =
                    "all committed layout moves were rolled back; earlier attribute transactions remain committed";
            }
        }
        furniture_auto_run_active_ = false;
        if (furniture_placement_gateway_) {
            furniture_retired_keys_ =
                furniture_placement_gateway_->BlockedWarehouseKeys();
        }
        Logger::Instance().Write(
            rollback_completed == rollback_total
                ? LogLevel::Info
                : LogLevel::Error,
            "FurniturePlacement",
            "AC3907",
            "Auto Place rollback: restored=" +
                std::to_string(rollback_completed) + "/" +
                std::to_string(rollback_total) + "; " + rollback_detail +
                ".");
        ClearFurnitureLayoutPreview();
        if (house_button_controller_) {
            house_button_controller_->SetState(
                OrganizeButtonState::Failed, reason);
        }
        Logger::Instance().Write(
            LogLevel::Warn,
            "FurniturePlacement",
            "AC3905",
            "Auto Place stopped after " + std::to_string(completed) +
                " committed moves; remaining=" +
                std::to_string(remaining) + ": " + reason +
                "; rollback=" + std::to_string(rollback_completed) +
                "/" + std::to_string(rollback_total));
        if (recommendation_marker_controller_ && furniture_mode_) {
            const bool english = config_runtime_ &&
                config_runtime_->Current().general.language == "en-US";
            const auto labels = english
                ? std::vector<std::string>{
                      "Auto Place stopped",
                      "Completed: " + std::to_string(completed) +
                          ", remaining: " + std::to_string(remaining),
                      "Rolled back: " +
                          std::to_string(rollback_completed) + "/" +
                          std::to_string(rollback_total),
                      reason,
                      "Run Start Analysis again to continue"}
                : std::vector<std::string>{
                      "自动放置已停止",
                      "已完成：" + std::to_string(completed) +
                          "，剩余：" + std::to_string(remaining),
                      "已回滚：" +
                          std::to_string(rollback_completed) + "/" +
                          std::to_string(rollback_total),
                      reason,
                      "请再次点击开始分析后继续"};
            (void)recommendation_marker_controller_->ShowFurnitureAnalysis(
                context.scene_generation, labels);
        }
    };

    if (!furniture_analysis_preview_ ||
        !furniture_placement_gateway_ ||
        !furniture_mode_ ||
        context.kind != UiContextKind::House ||
        !context.input_enabled ||
        context.save_in_progress ||
        context.scene_generation != furniture_execution_generation_) {
        fail("the House furniture scene changed during execution");
        return;
    }

    const auto& upgrades =
        furniture_analysis_preview_->attribute_upgrades;
    if (furniture_execution_upgrade_index_ < upgrades.size()) {
        const auto upgrade =
            upgrades[furniture_execution_upgrade_index_];
        const FurniturePlacementLocator placed_locator{
            upgrade.placed_item_id, upgrade.placed_stable_key};
        const auto placed =
            furniture_placement_gateway_->Locate(placed_locator);
        if (placed.status != FurniturePlacementLookupStatus::Found ||
            placed.room != upgrade.target_room_id) {
            fail("the placed furniture for an attribute upgrade is no longer bound to its analyzed room");
            return;
        }
        const auto replaced =
            furniture_placement_gateway_->ReplaceWithWarehouse({
                .placed = placed_locator,
                .warehouse_item = upgrade.warehouse_item_id,
                .warehouse_stable_key = upgrade.warehouse_stable_key,
                .target_x = upgrade.target_x,
                .target_y = upgrade.target_y,
                .support_dependents_top_down =
                    BuildSupportDependentRequests(upgrade)});
        if (replaced.status !=
            FurnitureWarehouseReplacementStatus::Replaced) {
            if (replaced.seh_code != 0U) {
                furniture_faulted_generation_ = context.scene_generation;
            }
            std::ostringstream rejection;
            rejection << "native warehouse replacement rejected "
                      << SafeTechnicalName(upgrade.placed_item_id)
                      << " key=" << upgrade.placed_stable_key
                      << " -> "
                      << SafeTechnicalName(upgrade.warehouse_item_id)
                      << " key=" << upgrade.warehouse_stable_key
                      << " status="
                      << FurnitureWarehouseReplacementStatusName(
                             replaced.status)
                      << " signature="
                      << (replaced.signatures_valid ? 1 : 0)
                      << " created="
                      << (replaced.warehouse_piece_created ? 1 : 0)
                      << " placement="
                      << (replaced.placement_valid ? 1 : 0)
                      << " committed=" << (replaced.committed ? 1 : 0)
                      << " verified=" << (replaced.verified ? 1 : 0)
                      << " support="
                      << replaced.support_dependents_stored << '/'
                      << replaced.support_dependent_count << "->"
                      << replaced.support_dependents_restored
                      << " native_rollback="
                      << (replaced.rollback_attempted ? 1 : 0) << '/'
                      << (replaced.rollback_succeeded ? 1 : 0)
                      << " seh=0x" << std::hex << std::uppercase
                      << replaced.seh_code << " rva=0x"
                      << replaced.exception_rva << std::dec << ": "
                      << replaced.message;
            fail(rejection.str());
            return;
        }
        Logger::Instance().Write(
            LogLevel::Info,
            "FurniturePlacement",
            "AC3912",
            "Auto Place attribute upgrade room=" +
                SafeTechnicalName(upgrade.target_room_id) +
                " placed=" + SafeTechnicalName(upgrade.placed_item_id) +
                " key=" + std::to_string(upgrade.placed_stable_key) +
                " -> warehouse=" +
                SafeTechnicalName(upgrade.warehouse_item_id) +
                " key=" + std::to_string(upgrade.warehouse_stable_key) +
                " placed_at=(" + std::to_string(replaced.target_x) + "," +
                std::to_string(replaced.target_y) + ") support=" +
                std::to_string(replaced.support_dependents_stored) + "/" +
                std::to_string(replaced.support_dependent_count) + "->" +
                std::to_string(replaced.support_dependents_restored) +
                " gain=" +
                CompactAttributeGain(upgrade.gain) + ".");
        ++furniture_execution_upgraded_;
        furniture_attribute_upgrade_committed_in_mode_ = true;
        furniture_execution_committed_upgrade_indices_.push_back(
            furniture_execution_upgrade_index_);
        ++furniture_execution_upgrade_index_;
        furniture_auto_run_next_transaction_ =
            std::chrono::steady_clock::now() +
            kFurnitureTransactionSettleDelay;
        if (furniture_execution_upgrade_index_ == upgrades.size() &&
            furniture_analysis_preview_->layout_plan.moves.empty()) {
            finish();
        }
        return;
    }

    const auto& plan = furniture_analysis_preview_->layout_plan;
    if (furniture_execution_index_ >= plan.moves.size()) {
        if (furniture_execution_index_ == plan.moves.size()) {
            finish();
        } else {
            fail("the furniture execution cursor exceeded the sealed plan");
        }
        return;
    }
    const auto move = plan.moves[furniture_execution_index_];
    const FurniturePlacementLocator locator{
        move.item_id, move.stable_key};
    if (furniture_planning::IsWarehouseLayoutMove(move)) {
        const auto placed =
            furniture_placement_gateway_->PlaceFromWarehouse({
                .warehouse_item = move.item_id,
                .warehouse_stable_key = move.stable_key,
                .target_room = move.target_room_id,
                .target_x = move.target_x,
                .target_y = move.target_y});
        if (placed.status != FurnitureWarehousePlacementStatus::Placed) {
            if (placed.seh_code != 0U) {
                furniture_faulted_generation_ = context.scene_generation;
            }
            std::ostringstream rejection;
            rejection << "native warehouse placement rejected "
                      << SafeTechnicalName(move.item_id)
                      << " key=" << move.stable_key
                      << " target="
                      << SafeTechnicalName(move.target_room_id)
                      << " (" << move.target_x << ',' << move.target_y << ')'
                      << " status="
                      << FurnitureWarehousePlacementStatusName(
                             placed.status)
                      << " signature="
                      << (placed.signatures_valid ? 1 : 0)
                      << " created="
                      << (placed.warehouse_piece_created ? 1 : 0)
                      << " placement="
                      << (placed.placement_valid ? 1 : 0)
                      << " committed=" << (placed.committed ? 1 : 0)
                      << " verified=" << (placed.verified ? 1 : 0)
                      << " native_rollback="
                      << (placed.rollback_attempted ? 1 : 0) << '/'
                      << (placed.rollback_succeeded ? 1 : 0)
                      << " seh=0x" << std::hex << std::uppercase
                      << placed.seh_code << " rva=0x"
                      << placed.exception_rva << std::dec << ": "
                      << placed.message;
            fail(rejection.str());
            return;
        }
        ++furniture_execution_moved_;
        furniture_attribute_upgrade_committed_in_mode_ = true;
        furniture_execution_committed_move_indices_.push_back(
            furniture_execution_index_);
        Logger::Instance().Write(
            LogLevel::Info,
            "FurniturePlacement",
            "AC3913",
            "Auto Place warehouse furniture=" +
                SafeTechnicalName(move.item_id) + " key=" +
                std::to_string(move.stable_key) + " target=" +
                SafeTechnicalName(move.target_room_id) + " (" +
                std::to_string(move.target_x) + "," +
                std::to_string(move.target_y) + ") status=" +
                FurnitureWarehousePlacementStatusName(placed.status));
        ++furniture_execution_index_;
        furniture_auto_run_next_transaction_ =
            std::chrono::steady_clock::now() +
            kFurnitureTransactionSettleDelay;
        if (furniture_execution_index_ == plan.moves.size()) {
            finish();
        }
        return;
    }
    const auto location = furniture_placement_gateway_->Locate(locator);
    if (location.status != FurniturePlacementLookupStatus::Found) {
        fail("the planned furniture instance is no longer available: " +
            location.message);
        return;
    }
    const bool already_at_target =
        location.room == move.target_room_id &&
        location.saved_x == move.target_x &&
        location.saved_y == move.target_y;
    if (!already_at_target && location.room != move.from_room_id) {
        fail("the planned furniture moved to another room before execution");
        return;
    }
    const bool relocated_attribute_upgrade = std::ranges::any_of(
        furniture_execution_committed_upgrade_indices_,
        [&upgrades, &move](const auto index) {
            return index < upgrades.size() &&
                upgrades[index].warehouse_stable_key == move.stable_key;
        });

    FurniturePlacementMoveStatus status{
        FurniturePlacementMoveStatus::AlreadyPlaced};
    std::string message{"the furniture was already at the planned coordinate"};
    auto committed_x = move.target_x;
    auto committed_y = move.target_y;
    if (!already_at_target) {
        if (!relocated_attribute_upgrade &&
            (location.saved_x != move.from_x ||
             location.saved_y != move.from_y)) {
            fail("the planned furniture coordinate changed before execution");
            return;
        }
        const auto moved = furniture_placement_gateway_->Move({
            locator,
            move.target_room_id,
            move.target_x,
            move.target_y,
            true});
        status = moved.status;
        message = moved.message;
        committed_x = moved.target_x;
        committed_y = moved.target_y;
        if (status != FurniturePlacementMoveStatus::Moved &&
            status != FurniturePlacementMoveStatus::AlreadyPlaced) {
            if (moved.seh_code != 0U) {
                furniture_faulted_generation_ = context.scene_generation;
            }
            const bool safely_rejected =
                furniture_auto_run_active_ && moved.seh_code == 0U &&
                furniture_execution_committed_move_indices_.empty() &&
                moved.rollback_attempted && moved.rollback_succeeded &&
                (status == FurniturePlacementMoveStatus::RejectedRestored ||
                 status == FurniturePlacementMoveStatus::FailedRestored);
            if (safely_rejected) {
                const auto blocked_room =
                    furniture_analysis_preview_->layout_plan.target_room_id;
                if (!blocked_room.empty() &&
                    std::ranges::find(
                        furniture_locked_room_ids_, blocked_room) ==
                        furniture_locked_room_ids_.end()) {
                    auto runtime_furniture = CaptureRuntimeFurnitureState(
                        current_house_scene_manager_);
                    if (runtime_furniture) {
                        LockFurnitureRoom(
                            furniture_locked_room_ids_,
                            furniture_locked_room_signatures_,
                            runtime_furniture.value,
                            blocked_room);
                    }
                }
                ++furniture_auto_run_blocked_rooms_;
                Logger::Instance().Write(
                    LogLevel::Warn,
                    "FurniturePlacement",
                    "AC3923",
                    "Continuous Auto Place deferred the native-rejected room and will re-plan: room=" +
                        SafeTechnicalName(blocked_room) +
                        " item=" +
                        SafeTechnicalName(move.item_id) +
                        " key=" + std::to_string(move.stable_key) +
                        " room=" + SafeTechnicalName(move.from_room_id) +
                        "->" + SafeTechnicalName(move.target_room_id) +
                        " status=" + FurniturePlacementMoveStatusName(status) +
                        ".");
                ClearFurnitureLayoutPreview();
                StartFurnitureAnalysis(context.scene_generation);
                return;
            }
            std::ostringstream rejection;
            rejection << "native placement rejected " << move.item_id
                      << " room=" << SafeTechnicalName(move.from_room_id)
                      << "->" << SafeTechnicalName(move.target_room_id)
                      << " from=(" << move.from_x << ',' << move.from_y
                      << ") target=(" << move.target_x << ','
                      << move.target_y << ") status="
                      << FurniturePlacementMoveStatusName(status)
                      << " signature=" << (moved.signatures_valid ? 1 : 0)
                      << " placement=" << (moved.placement_valid ? 1 : 0)
                      << " committed=" << (moved.committed ? 1 : 0)
                      << " verified=" << (moved.verified ? 1 : 0)
                      << " native_rollback="
                      << (moved.rollback_attempted ? 1 : 0) << '/'
                      << (moved.rollback_succeeded ? 1 : 0)
                      << " seh=0x" << std::hex << std::uppercase
                      << moved.seh_code << " rva=0x"
                      << moved.exception_rva << std::dec << ": "
                      << message;
            fail(rejection.str());
            return;
        }
        if (status == FurniturePlacementMoveStatus::Moved) {
            ++furniture_execution_moved_;
            furniture_focus_room_id_ = move.target_room_id;
            furniture_execution_committed_move_indices_.push_back(
                furniture_execution_index_);
        }
    }

    Logger::Instance().Write(
        LogLevel::Info,
        "FurniturePlacement",
        "AC3903",
        "Auto Place item=" + move.item_id +
            " key=" + std::to_string(move.stable_key) +
            " room=" + SafeTechnicalName(move.from_room_id) + "->" +
            SafeTechnicalName(move.target_room_id) +
            " from=(" + std::to_string(move.from_x) + "," +
            std::to_string(move.from_y) + ") planned=(" +
            std::to_string(move.target_x) + "," +
            std::to_string(move.target_y) + ") committed=(" +
            std::to_string(committed_x) + "," +
            std::to_string(committed_y) + ") status=" +
            FurniturePlacementMoveStatusName(status));
    ++furniture_execution_index_;
    furniture_auto_run_next_transaction_ =
        std::chrono::steady_clock::now() +
        kFurnitureTransactionSettleDelay;
    if (furniture_execution_index_ != plan.moves.size()) {
        return;
    }
    finish();
}

#ifdef _DEBUG
void MewUiBridge::RunFurnitureNativeMoveTest(
    const UiContextSnapshot& context) {
    if (!furniture_mode_ ||
        context.kind != UiContextKind::House ||
        !context.input_enabled ||
        context.save_in_progress ||
        !furniture_placement_gateway_) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "FurniturePlacement",
            "AC3800",
            "F8 native furniture move test requires the active furniture placement screen.");
        return;
    }

    const bool english = config_runtime_ &&
        config_runtime_->Current().general.language == "en-US";
    const FurniturePlacementLocator locator{
        "object_cattree1", std::uint64_t{5}};
    const auto location = furniture_placement_gateway_->Locate(locator);
    if (location.status != FurniturePlacementLookupStatus::Found) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "FurniturePlacement",
            "AC3801",
            "F8 native furniture lookup failed: " + location.message);
        if (recommendation_marker_controller_) {
            const auto labels = english
                ? std::vector<std::string>{
                      "Native furniture move unavailable",
                      "Cat Tree A was not uniquely identified",
                      location.message,
                      "No furniture was moved"}
                : std::vector<std::string>{
                      "原生家具移动不可用",
                      "未能唯一识别猫爬架 A",
                      location.message,
                      "没有移动任何家具"};
            (void)recommendation_marker_controller_->ShowFurnitureAnalysis(
                context.scene_generation, labels);
        }
        return;
    }

    const std::array<std::pair<std::int32_t, std::int32_t>, 2> targets =
        location.saved_x == 3 && location.saved_y == -9
            ? std::array<std::pair<std::int32_t, std::int32_t>, 2>{
                  std::pair<std::int32_t, std::int32_t>{-6, -7},
                  std::pair<std::int32_t, std::int32_t>{3, -9}}
            : std::array<std::pair<std::int32_t, std::int32_t>, 2>{
                  std::pair<std::int32_t, std::int32_t>{3, -9},
                  std::pair<std::int32_t, std::int32_t>{-6, -7}};

    FurniturePlacementMoveResult moved;
    bool attempted{};
    for (const auto& [target_x, target_y] : targets) {
        if (target_x == location.saved_x &&
            target_y == location.saved_y) {
            continue;
        }
        attempted = true;
        moved = furniture_placement_gateway_->MoveSameRoom({
            .locator = locator,
            .target_x = target_x,
            .target_y = target_y});
        if (moved.status !=
            FurniturePlacementMoveStatus::RejectedRestored) {
            break;
        }
    }
    if (!attempted) {
        moved.status = FurniturePlacementMoveStatus::AlreadyPlaced;
        moved.stable_key = location.stable_key;
        moved.from_x = location.saved_x;
        moved.from_y = location.saved_y;
        moved.target_x = location.saved_x;
        moved.target_y = location.saved_y;
        moved.verified = true;
        moved.message = "no alternate test coordinate was available";
    }

    std::ostringstream detail;
    detail << "F8 native furniture move status="
           << FurniturePlacementMoveStatusName(moved.status)
           << " item=object_cattree1 key=" << moved.stable_key
           << " from=(" << moved.from_x << ',' << moved.from_y << ')'
           << " target=(" << moved.target_x << ',' << moved.target_y << ')'
           << " signatures=" << (moved.signatures_valid ? 1 : 0)
           << " placement=" << (moved.placement_valid ? 1 : 0)
           << " committed=" << (moved.committed ? 1 : 0)
           << " verified=" << (moved.verified ? 1 : 0)
           << " rollback=" << (moved.rollback_attempted ? 1 : 0)
           << '/' << (moved.rollback_succeeded ? 1 : 0)
           << " exception=0x" << std::hex << moved.seh_code
           << " exception_rva=0x" << moved.exception_rva << std::dec;
    Logger::Instance().Write(
        moved.status == FurniturePlacementMoveStatus::Moved
            ? LogLevel::Info
            : moved.status == FurniturePlacementMoveStatus::RestoreFailed
                ? LogLevel::Error
                : LogLevel::Warn,
        "FurniturePlacement",
        "AC3802",
        detail.str());

    if (!recommendation_marker_controller_) {
        return;
    }
    std::vector<std::string> labels;
    if (moved.status == FurniturePlacementMoveStatus::Moved) {
        const auto coordinates =
            "(" + std::to_string(moved.from_x) + "," +
            std::to_string(moved.from_y) + ") -> (" +
            std::to_string(moved.target_x) + "," +
            std::to_string(moved.target_y) + ")";
        labels = english
            ? std::vector<std::string>{
                  "Native furniture move succeeded",
                  "Cat Tree A " + coordinates,
                  "Game validation and commit both passed",
                  "Save, exit, and re-enter to confirm persistence"}
            : std::vector<std::string>{
                  "原生家具移动成功",
                  "猫爬架 A " + coordinates,
                  "游戏原生校验与提交均已通过",
                  "请保存、退出并重进确认持久化"};
    } else if (moved.status ==
               FurniturePlacementMoveStatus::RestoreFailed) {
        labels = english
            ? std::vector<std::string>{
                  "Furniture move restore was not verified",
                  "Do not save this test session",
                  "Exit the game and re-enter the save",
                  "See AC3802 in the AutoCattery log"}
            : std::vector<std::string>{
                  "家具原位置回滚未确认",
                  "本次测试请不要保存",
                  "请退出游戏后重新进入存档",
                  "详情见日志 AC3802"};
    } else {
        labels = english
            ? std::vector<std::string>{
                  "Native furniture move did not commit",
                  moved.message,
                  moved.rollback_succeeded
                      ? "The original placement was restored"
                      : "No native removal was performed",
                  "See AC3802 in the AutoCattery log"}
            : std::vector<std::string>{
                  "原生家具移动未提交",
                  moved.message,
                  moved.rollback_succeeded
                      ? "原位置已经恢复"
                      : "未执行原生移除",
                  "详情见日志 AC3802"};
    }
    (void)recommendation_marker_controller_->ShowFurnitureAnalysis(
        context.scene_generation, labels);
}
#endif

void MewUiBridge::ObserveMappingProbe(
    const UiContextSnapshot& context,
    const std::vector<RuntimeScene>& scenes) {
    if (mapping_probe_session_.ShouldSample(context)) {
        const auto scene = std::find_if(
            scenes.begin(),
            scenes.end(),
            [&context](const RuntimeScene& candidate) {
                return candidate.ready &&
                       candidate.name == context.scene_name;
            });
        if (scene == scenes.end()) {
            return;
        }
        const auto native =
            AcMewInspectAnonymousMapping(scene->manager);
        mapping_probe_session_.Observe(
            context,
            {
                native.component_count,
                native.typed_component_count,
                native.type_name_count,
                native.button_count,
                native.role_count,
                native.type_digest,
                native.role_digest
            });
    } else if (mapping_probe_session_.Armed() &&
               context.kind == UiContextKind::UnsafeTransition) {
        mapping_probe_session_.Observe(context, {});
    }

    if (mapping_probe_session_.Complete() && !mapping_probe_logged_) {
        mapping_probe_logged_ = true;
        const auto& summary = mapping_probe_session_.Summary();
        std::ostringstream message;
        message << "MappingUnavailable request="
                << mapping_probe_request_sequence_
                << " generation="
                << summary.scene_generation
                << " anonymous_components="
                << summary.observation.component_count
                << " typed_components="
                << summary.observation.typed_component_count
                << " type_names="
                << summary.observation.type_name_count
                << " buttons=" << summary.observation.button_count
                << " stable_roles="
                << (summary.stable_component_roles ? 1 : 0)
                << " type_digest=" << std::hex
                << summary.observation.type_digest
                << " role_digest="
                << summary.observation.role_digest << std::dec
                << " stable_cat_id_boundary=0 visual_marker_boundary=0";
        Logger::Instance().Write(
            LogLevel::Info,
            "RecommendationProbe",
            "AC12102",
            message.str());
        const auto scene = std::find_if(
            scenes.begin(),
            scenes.end(),
            [&context](const RuntimeScene& candidate) {
                return candidate.ready &&
                       candidate.name == context.scene_name;
            });
        if (scene != scenes.end()) {
            std::array<
                AcMewMappingTypeRecord,
                kMappingRecordCapacity> types{};
            const auto type_count = AcMewEnumerateMappingTypes(
                scene->manager,
                types.data(),
                types.size());
            for (std::size_t index = 0; index < type_count; ++index) {
                std::ostringstream detail;
                detail << "request=" << mapping_probe_request_sequence_
                       << " generation=" << summary.scene_generation
                       << " type="
                       << SafeTechnicalName(types[index].type_name)
                       << " components=" << types[index].component_count
                       << " roots=" << types[index].root_node_count;
                Logger::Instance().Write(
                    LogLevel::Info,
                    "RecommendationProbe",
                    "AC12103",
                    detail.str());
            }

            std::array<
                AcMewMappingRoleRecord,
                kMappingRecordCapacity> roles{};
            const auto role_count = AcMewEnumerateButtonRoles(
                scene->manager,
                roles.data(),
                roles.size());
            for (std::size_t index = 0; index < role_count; ++index) {
                std::ostringstream detail;
                detail << "request=" << mapping_probe_request_sequence_
                       << " generation=" << summary.scene_generation
                       << " role="
                       << SafeTechnicalName(roles[index].role_name)
                       << " buttons=" << roles[index].button_count;
                Logger::Instance().Write(
                    LogLevel::Info,
                    "RecommendationProbe",
                    "AC12104",
                    detail.str());
            }
        }
    }
}

void MewUiBridge::StartMappingSnapshotAttempt() {
    if (!mapping_snapshot_request_active_ ||
        mapping_snapshot_task_.valid()) {
        return;
    }
    mapping_snapshot_task_sequence_ = mapping_snapshot_request_sequence_;
    mapping_snapshot_task_generation_ = mapping_snapshot_generation_;
    ++mapping_snapshot_attempt_;
    const auto generation = mapping_snapshot_task_generation_;
    mapping_snapshot_task_ = std::async(
        std::launch::async,
        [generation] {
            snapshot::SaveSnapshotAdapter adapter;
            return adapter.CaptureHouseSnapshotCandidates(generation);
        });
}

void MewUiBridge::ObserveHouseCatIdentity(
    const UiContextSnapshot& context,
    const std::vector<RuntimeScene>& scenes) {
    if (mapping_identity_logged_ ||
        !mapping_snapshot_request_active_ ||
        !mapping_probe_session_.Complete()) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (!mapping_snapshot_task_.valid()) {
        if (mapping_snapshot_next_attempt_.time_since_epoch().count() == 0 ||
            now >= mapping_snapshot_next_attempt_) {
            StartMappingSnapshotAttempt();
        }
        return;
    }
    if (mapping_snapshot_task_.wait_for(std::chrono::milliseconds(0)) !=
        std::future_status::ready) {
        return;
    }

    const auto task_sequence = mapping_snapshot_task_sequence_;
    const auto task_generation = mapping_snapshot_task_generation_;
    const auto captured = mapping_snapshot_task_.get();
    if (task_sequence != mapping_snapshot_request_sequence_ ||
        task_generation != mapping_snapshot_generation_) {
        mapping_snapshot_next_attempt_ = {};
        return;
    }

    const bool retry_available =
        context.kind == UiContextKind::House &&
        context.input_enabled &&
        !context.save_in_progress &&
        context.scene_generation == mapping_snapshot_generation_ &&
        now < mapping_snapshot_retry_deadline_;
    const auto retry_or_complete = [&] {
        if (retry_available) {
            mapping_snapshot_next_attempt_ =
                now + kMappingSnapshotRetryDelay;
            return;
        }
        mapping_snapshot_request_active_ = false;
        mapping_identity_logged_ = true;
        mapping_snapshot_next_attempt_ = {};
        recommendation_marker_controller_->CompleteProbe(
            mapping_snapshot_generation_);
    };

    std::ostringstream message;
    message << "request=" << mapping_snapshot_request_sequence_
            << " attempt=" << mapping_snapshot_attempt_
            << " generation=" << mapping_snapshot_generation_;
    if (!captured || captured.value.empty()) {
        message << " snapshot_valid=0 candidates=0 house_cats=0 requested_ids=0"
                << " layouts=0 stable_bijection=0 retry_pending="
                << (retry_available ? 1 : 0);
        Logger::Instance().Write(
            LogLevel::Info,
            "RecommendationProbe",
            "AC12105",
            message.str());
        retry_or_complete();
        return;
    }

    const auto scene = std::find_if(
        scenes.begin(),
        scenes.end(),
        [&context](const RuntimeScene& candidate) {
            return candidate.ready &&
                   candidate.name == context.scene_name;
        });
    if (scene == scenes.end() ||
        context.kind != UiContextKind::House ||
        context.scene_generation != mapping_snapshot_generation_) {
        message << " snapshot_valid=1 candidates="
                << captured.value.size()
                << " house_cats=0 requested_ids=0"
                << " layouts=0 stable_bijection=0 retry_pending="
                << (retry_available ? 1 : 0);
        Logger::Instance().Write(
            LogLevel::Info,
            "RecommendationProbe",
            "AC12105",
            message.str());
        retry_or_complete();
        return;
    }

    const snapshot::HouseSnapshot* selected_snapshot{};
    AcMewHouseCatIdentityProbe identity{};
    std::ptrdiff_t roots{};
    bool coverage_ready{};
    bool selected_exact{};
    bool diagnostic_available{};
    std::size_t selected_match_count{};
    std::vector<AcMewHouseCatMatch> selected_matches;
    for (const auto& candidate : captured.value) {
        if (!candidate.capabilities.stable_cat_id ||
            candidate.scene_generation != mapping_snapshot_generation_) {
            continue;
        }
        std::vector<std::int64_t> cat_ids;
        cat_ids.reserve(candidate.cats.size());
        for (const auto& cat : candidate.cats) {
            cat_ids.push_back(cat.id);
        }
        std::vector<AcMewHouseCatMatch> candidate_matches(
            candidate.cats.size());
        const auto candidate_identity = AcMewProbeHouseCatIdentity(
            scene->manager,
            cat_ids.data(),
            cat_ids.size(),
            candidate_matches.data(),
            candidate_matches.size());
        const auto candidate_roots = std::count_if(
            candidate_matches.begin(),
            candidate_matches.begin() +
                static_cast<std::ptrdiff_t>(candidate_identity.match_count),
            [](const AcMewHouseCatMatch& match) {
                return match.root_node != nullptr;
            });
        const bool candidate_coverage =
            candidate_identity.house_cat_count > 0 &&
            candidate_identity.match_count *
                    kMinimumMappedCoverageDenominator >=
                static_cast<std::size_t>(
                    candidate_identity.house_cat_count) *
                    kMinimumMappedCoverageNumerator;
        const bool candidate_ready =
            candidate_identity.stable_bijection != 0 &&
            candidate_identity.consistent_mapping != 0 &&
            candidate_identity.match_count == candidate.cats.size() &&
            candidate_roots == static_cast<std::ptrdiff_t>(
                candidate_identity.match_count) &&
            candidate_coverage;
        const bool candidate_exact = candidate_ready &&
            candidate_identity.match_count ==
                candidate_identity.house_cat_count;
        if (selected_snapshot == nullptr &&
            (!diagnostic_available ||
             candidate_identity.match_count > identity.match_count)) {
            diagnostic_available = true;
            identity = candidate_identity;
            roots = candidate_roots;
            coverage_ready = candidate_coverage;
        }
        if (!candidate_ready) {
            continue;
        }
        if (selected_snapshot == nullptr ||
            (candidate_exact && !selected_exact) ||
            (candidate_exact == selected_exact &&
             candidate_identity.match_count > selected_match_count)) {
            selected_snapshot = &candidate;
            identity = candidate_identity;
            roots = candidate_roots;
            coverage_ready = candidate_coverage;
            selected_exact = candidate_exact;
            selected_match_count = candidate_identity.match_count;
            selected_matches = std::move(candidate_matches);
        }
        if (candidate_exact) {
            break;
        }
    }
    message << " snapshot_valid=1 candidates="
            << captured.value.size()
            << " house_cats="
            << identity.house_cat_count
            << " requested_ids=" << identity.requested_cat_count
            << " layouts=" << identity.valid_layout_count
            << " offset=" << identity.first_identity_offset
            << " width=" << static_cast<unsigned>(
                   identity.first_identity_width)
            << " consistent=" << static_cast<unsigned>(
                   identity.consistent_mapping)
            << " matched=" << identity.match_count
            << " roots=" << roots
            << " coverage_ready=" << (coverage_ready ? 1 : 0)
            << " selected_exact=" << (selected_exact ? 1 : 0)
            << " stable_bijection=" << static_cast<unsigned>(
                   identity.stable_bijection)
            << " visual_marker_boundary=0";
    Logger::Instance().Write(
        LogLevel::Info,
        "RecommendationProbe",
        "AC12105",
        message.str());

    if (selected_snapshot == nullptr) {
        retry_or_complete();
        return;
    }

    auto scoring_config = recommendation_scoring_config_;
    // Class identity excludes cats already spent in a prior expedition, and
    // the persisted death day excludes dead cats. Other life-stage thresholds
    // and injury remain unavailable, so preserve those limitations.
    scoring_config.require_confirmed_eligibility = false;
    scoring_config.recommended_count = std::min({
        scoring_config.recommended_count,
        recommendation_marker_config_.recommended_count,
        selected_snapshot->cats.size()
    });
    const auto dead_cats = std::count_if(
        selected_snapshot->cats.begin(),
        selected_snapshot->cats.end(),
        [](const snapshot::CatSnapshot& cat) {
            return cat.life_stage == snapshot::LifeStage::Dead;
        });
    const auto classed_cats = std::count_if(
        selected_snapshot->cats.begin(),
        selected_snapshot->cats.end(),
        [](const snapshot::CatSnapshot& cat) {
            return cat.class_id != "Colorless";
        });
    const auto ranking =
        scoring::RankCombatCats(*selected_snapshot, scoring_config);
    if (!ranking || ranking.value.recommended_cat_ids.empty()) {
        mapping_snapshot_request_active_ = false;
        mapping_identity_logged_ = true;
        Logger::Instance().Write(
            LogLevel::Info,
            "RecommendationMarker",
            "AC12106",
            "marked=0 excluded_dead=" + std::to_string(dead_cats) +
            " excluded_classed=" + std::to_string(classed_cats) +
            " stable_cat_id_boundary=1 "
            "visual_fallback=clickable_list "
            "expedition_selection_changed=0");
        recommendation_marker_controller_->CompleteProbe(
            mapping_snapshot_generation_);
        return;
    }

    std::vector<std::string> labels;
    std::vector<void*> detail_targets;
    labels.reserve(ranking.value.recommended_cat_ids.size());
    detail_targets.reserve(ranking.value.recommended_cat_ids.size());
    std::size_t marked{};
    for (const auto cat_id : ranking.value.recommended_cat_ids) {
        const auto cat = std::find_if(
            selected_snapshot->cats.begin(),
            selected_snapshot->cats.end(),
            [cat_id](const snapshot::CatSnapshot& candidate) {
                return candidate.id == cat_id;
            });
        const auto score = std::find_if(
            ranking.value.ranked.begin(),
            ranking.value.ranked.end(),
            [cat_id](const scoring::CombatScoreResult& candidate) {
                return candidate.cat_id == cat_id;
            });
        const auto mapped = std::find_if(
            selected_matches.begin(),
            selected_matches.begin() +
                static_cast<std::ptrdiff_t>(selected_match_count),
            [cat_id](const AcMewHouseCatMatch& candidate) {
                return candidate.cat_id == cat_id &&
                       candidate.root_node != nullptr;
            });
        if (cat == selected_snapshot->cats.end() ||
            score == ranking.value.ranked.end() ||
            mapped == selected_matches.begin() +
                static_cast<std::ptrdiff_t>(selected_match_count)) {
            continue;
        }
        ++marked;
        std::ostringstream label;
        if (recommendation_marker_config_.show_rank) {
            label << marked << ' ';
        }
        label << SafeDisplayName(cat->display_name);
        if (recommendation_marker_config_.show_score) {
            label << ' ' << std::fixed << std::setprecision(1)
                  << score->score;
        }
        label << " ?";
        labels.push_back(label.str());
        detail_targets.push_back(mapped->component);
    }

    if (marked == 0) {
        mapping_snapshot_request_active_ = false;
        mapping_identity_logged_ = true;
        recommendation_marker_controller_->CompleteProbe(
            mapping_snapshot_generation_);
        return;
    }
    const auto shown =
        recommendation_marker_controller_->ShowRecommendations(
            mapping_snapshot_generation_,
            labels);
    if (!shown) {
        mapping_snapshot_request_active_ = false;
        mapping_identity_logged_ = true;
        Logger::Instance().Write(
            LogLevel::Warn,
            "RecommendationMarker",
            "AC12108",
            "Read-only recommendation summary could not be attached to "
            "the current House scene.");
        recommendation_marker_controller_->CompleteProbe(
            mapping_snapshot_generation_);
        return;
    }
    mapping_snapshot_request_active_ = false;
    mapping_identity_logged_ = true;
    recommendation_detail_targets_ = std::move(detail_targets);
    Logger::Instance().Write(
        LogLevel::Info,
        "RecommendationMarker",
        "AC12106",
        "marked=" + std::to_string(marked) +
            " excluded_dead=" + std::to_string(dead_cats) +
            " excluded_classed=" + std::to_string(classed_cats) +
            " mapped_house_cats=" + std::to_string(identity.match_count) +
            "/" + std::to_string(identity.house_cat_count) +
            " stable_cat_id_boundary=1 visual_fallback=clickable_list "
            "expedition_selection_changed=0");
}

SceneObservation MewUiBridge::ObserveScenes(
    const std::vector<RuntimeScene>& scenes) const {
    const auto save_scene = std::find_if(
        scenes.begin(),
        scenes.end(),
        [](const RuntimeScene& scene) {
            return ClassifyBlockingOverlay(scene.name, scene.ready) ==
                   BlockingOverlayKind::SaveInProgress;
        });
    if (save_scene != scenes.end()) {
        return {
            UiContextKind::UnsafeTransition,
            save_scene->name,
            reinterpret_cast<std::uintptr_t>(save_scene->manager),
            false,
            false,
            true,
            {"save-scene:" + save_scene->name}
        };
    }

    const auto match_rule =
        [&scenes](UiContextKind kind, const SceneSignatureRule& rule)
        -> std::vector<SceneObservation> {
        std::vector<SceneObservation> matches;
        const bool has_structure =
            !rule.required_nodes_any.empty() ||
            !rule.required_nodes_all.empty();
        for (const auto& scene : scenes) {
            const bool name_match = Contains(rule.scene_names, scene.name);
            if (!name_match &&
                !(rule.allow_structure_match && has_structure)) {
                continue;
            }

            std::vector<std::string> matched;
            if (name_match) {
                matched.push_back("scene:" + scene.name);
            }

            bool any_match = rule.required_nodes_any.empty();
            for (const auto& node : rule.required_nodes_any) {
                if (MewUI_FindNodeInSceneByName(scene.manager, node.c_str())) {
                    any_match = true;
                    matched.push_back("node:" + node);
                }
            }

            bool all_match = true;
            for (const auto& node : rule.required_nodes_all) {
                if (MewUI_FindNodeInSceneByName(scene.manager, node.c_str())) {
                    matched.push_back("node:" + node);
                } else {
                    all_match = false;
                }
            }

            bool forbidden_match = false;
            for (const auto& node : rule.forbidden_nodes) {
                if (MewUI_FindNodeInSceneByName(scene.manager, node.c_str())) {
                    forbidden_match = true;
                    matched.push_back("forbidden:" + node);
                }
            }

            if (scene.ready && any_match && all_match && !forbidden_match) {
                matches.push_back({
                    kind,
                    scene.name,
                    reinterpret_cast<std::uintptr_t>(scene.manager),
                    true,
                    true,
                    false,
                    std::move(matched)
                });
            }
        }
        return matches;
    };

    auto house = match_rule(UiContextKind::House, signatures_.house);
    auto embark = match_rule(
        UiContextKind::EmbarkSelection,
        signatures_.embark_selection);
    if (house.size() + embark.size() != 1) {
        return {
            house.empty() && embark.empty()
                ? UiContextKind::Unknown
                : UiContextKind::UnsafeTransition,
            {},
            0,
            false,
            false,
            false,
            {"ambiguous-or-unmatched-scene"}
        };
    }
    return !house.empty() ? std::move(house.front())
                          : std::move(embark.front());
}

void MewUiBridge::LogSceneSummary(
    const std::vector<RuntimeScene>& scenes) {
    std::ostringstream summary;
    for (const auto& scene : scenes) {
        summary << scene.name << ':' << (scene.ready ? 1 : 0) << ';';
    }
    if (summary.str() == last_scene_summary_) {
        return;
    }
    last_scene_summary_ = summary.str();
    Logger::Instance().Write(
        LogLevel::Debug,
        "SceneProbe",
        "AC2400",
        "Loaded scene summary changed: " + last_scene_summary_);
}

void MewUiBridge::ExportSceneSummary(
    const std::vector<RuntimeScene>& scenes) const {
    std::error_code error;
    std::filesystem::create_directories(diagnostics_root_, error);
    if (error) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "SceneProbe",
            "AC2402",
            "Could not create diagnostics directory.");
        return;
    }

    nlohmann::json root{
        {"schema_version", 1},
        {"note", "Scene names and component counts only; no game assets exported."},
        {"scenes", nlohmann::json::array()}
    };
    for (const auto& scene : scenes) {
        root["scenes"].push_back({
            {"name", scene.name},
            {"ready", scene.ready},
            {"component_count", scene.component_count}
        });
    }

    const auto filename =
        "ui-scene-summary-" + TimestampForFilename() + ".json";
    std::ofstream output(diagnostics_root_ / filename, std::ios::trunc);
    output << root.dump(2);
    Logger::Instance().Write(
        LogLevel::Info,
        "SceneProbe",
        "AC2401",
        "Exported diagnostics scene summary: " + filename);
}

}  // namespace autocattery::ui
