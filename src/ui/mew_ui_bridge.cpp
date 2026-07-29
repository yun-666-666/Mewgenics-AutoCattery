#include "auto_cattery/ui/mew_ui_bridge.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "auto_cattery/config.hpp"
#include "auto_cattery/logger.hpp"
#include "auto_cattery/recommendation/snapshot_reader.hpp"
#include "auto_cattery/scoring/combat_ranker.hpp"
#include "auto_cattery/snapshot/save_snapshot_adapter.hpp"
#include "auto_cattery/ui/house_button_controller.hpp"
#include "auto_cattery/ui/recommendation_marker_controller.hpp"
#include "auto_cattery/workflow/organize_workflow_facade.hpp"
#include "mew_ui_house_button_view.hpp"
#include "mew_ui_house_cat_probe.h"
#include "mew_ui_house_detail_adapter.h"
#include "mew_ui_mapping_probe.h"
#include "mew_ui_recommendation_marker_view.hpp"
#include "mew_ui_scene_probe.h"
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#include "mew_ui_api.h"

namespace autocattery::ui {
namespace {

constexpr std::size_t kSceneProbeCapacity = 64;
constexpr std::size_t kMappingRecordCapacity = 128;

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
    mapping_snapshot_generation_ = 0;
    next_house_attach_retry_ = {};
    next_recommendation_attach_retry_ = {};
    house_button_view_ = std::make_unique<MewUiHouseButtonView>();
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
            mapping_snapshot_task_ = std::async(
                std::launch::async,
                [generation] {
                    snapshot::SaveSnapshotAdapter adapter;
                    return adapter.CaptureHouseSnapshot(generation);
                });
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
                    << " house=" << (unsigned)opened.house_unique
                    << " cat=" << (unsigned)opened.cat_valid
                    << " opened=" << (unsigned)opened.invoked
                    << " box_changed=0 expedition_selection_changed=0";
            Logger::Instance().Write(
                opened.invoked ? LogLevel::Info : LogLevel::Warn,
                "RecommendationMarker",
                "AC12109",
                message.str());
        });

    const auto config = LoadConfig(
        context.mod_root / L"config" / L"default_config.json",
        context.mod_root / L"config" / L"user_config.json");
    recommendation_scoring_config_ =
        config ? config.value.combat_scoring
               : scoring::CombatScoringConfig{};
    organize_workflow_ =
        std::make_unique<workflow::OrganizeWorkflowFacade>(
            std::make_unique<snapshot::SaveSnapshotAdapter>(),
            config ? config.value : Config{});
    house_button_controller_ = std::make_unique<HouseButtonController>(
        *house_button_view_,
        *organize_workflow_);
#ifdef _DEBUG
    debug_probe_enabled_ = true;
#else
    debug_probe_enabled_ =
        static_cast<bool>(config) && config.value.ui.show_debug_overlay;
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
            recommendation_detail_targets_.clear();
            house_button_controller_->Detach();
            recommendation_marker_controller_->Detach();
            next_house_attach_retry_ = {};
            last_house_attach_error_.clear();
            next_recommendation_attach_retry_ = {};
            last_recommendation_attach_error_.clear();
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
    if (house_button_controller_) {
        house_button_controller_->Detach();
    }
    if (recommendation_marker_controller_) {
        recommendation_marker_controller_->Detach();
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
    mapping_snapshot_generation_ = 0;
    recommendation_detail_targets_.clear();
    ready_logged_.store(false);
    house_button_controller_.reset();
    recommendation_marker_controller_.reset();
    recommendation_marker_view_.reset();
    organize_workflow_.reset();
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
    if (house_button_controller_) {
        house_button_controller_->Poll();
    }
    if (recommendation_marker_controller_) {
        recommendation_marker_controller_->Poll();
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

    if (debug_probe_enabled_) {
        LogSceneSummary(scenes);
        if ((GetAsyncKeyState(VK_F8) & 1) != 0) {
            ExportSceneSummary(scenes);
        }
    }
    (void)scene_context_.Observe(ObserveScenes(scenes));

    const auto context = scene_context_.Current();
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
    const bool interstitial_ready = scene_ready("Interstitial");
    const bool expedition_ready =
        scene_ready("Map") || scene_ready("Battle");
    recommendation_marker_controller_->ObserveRuntime(
        house_ready,
        interstitial_ready,
        expedition_ready);

    if (context.kind == UiContextKind::House &&
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

    if (house_ready &&
        recommendation_marker_controller_->ShouldShow() &&
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

void MewUiBridge::ObserveHouseCatIdentity(
    const UiContextSnapshot& context,
    const std::vector<RuntimeScene>& scenes) {
    if (mapping_identity_logged_ ||
        !mapping_probe_session_.Complete() ||
        !mapping_snapshot_task_.valid() ||
        mapping_snapshot_task_.wait_for(std::chrono::milliseconds(0)) !=
            std::future_status::ready) {
        return;
    }
    mapping_identity_logged_ = true;
    const auto captured = mapping_snapshot_task_.get();
    std::ostringstream message;
    message << "request=" << mapping_snapshot_request_sequence_
            << " generation=" << mapping_snapshot_generation_;
    if (!captured ||
        !captured.value.capabilities.stable_cat_id ||
        captured.value.scene_generation != mapping_snapshot_generation_) {
        message << " snapshot_valid=0 house_cats=0 requested_ids=0"
                << " layouts=0 stable_bijection=0";
        Logger::Instance().Write(
            LogLevel::Info,
            "RecommendationProbe",
            "AC12105",
            message.str());
        recommendation_marker_controller_->CompleteProbe(
            mapping_snapshot_generation_);
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
        message << " snapshot_valid=1 house_cats=0 requested_ids="
                << captured.value.cats.size()
                << " layouts=0 stable_bijection=0";
        Logger::Instance().Write(
            LogLevel::Info,
            "RecommendationProbe",
            "AC12105",
            message.str());
        recommendation_marker_controller_->CompleteProbe(
            mapping_snapshot_generation_);
        return;
    }

    std::vector<std::int64_t> cat_ids;
    cat_ids.reserve(captured.value.cats.size());
    for (const auto& cat : captured.value.cats) {
        cat_ids.push_back(cat.id);
    }
    const auto identity = AcMewProbeHouseCatIdentity(
        scene->manager,
        cat_ids.data(),
        cat_ids.size());
    const auto roots = std::count_if(
        identity.matches,
        identity.matches + identity.match_count,
        [](const AcMewHouseCatMatch& match) {
            return match.root_node != nullptr;
        });
    message << " snapshot_valid=1 house_cats="
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
            << " stable_bijection=" << static_cast<unsigned>(
                   identity.stable_bijection)
            << " visual_marker_boundary=0";
    Logger::Instance().Write(
        LogLevel::Info,
        "RecommendationProbe",
        "AC12105",
        message.str());

    const bool identity_ready =
        identity.stable_bijection != 0 &&
        identity.consistent_mapping != 0 &&
        identity.match_count == captured.value.cats.size() &&
        roots == static_cast<std::ptrdiff_t>(identity.match_count);
    if (!identity_ready) {
        recommendation_marker_controller_->CompleteProbe(
            mapping_snapshot_generation_);
        return;
    }

    auto scoring_config = recommendation_scoring_config_;
    // This build's save reader does not expose life-stage, injury, or combat
    // availability yet. Preserve all confirmed exclusions, but allow the
    // Stage 6 scorer to produce explicitly low-confidence visual suggestions.
    scoring_config.require_confirmed_eligibility = false;
    const auto ranking =
        scoring::RankCombatCats(captured.value, scoring_config);
    if (!ranking || ranking.value.recommended_cat_ids.empty()) {
        Logger::Instance().Write(
            LogLevel::Info,
            "RecommendationMarker",
            "AC12106",
            "marked=0 stable_cat_id_boundary=1 "
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
            captured.value.cats.begin(),
            captured.value.cats.end(),
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
            identity.matches,
            identity.matches + identity.match_count,
            [cat_id](const AcMewHouseCatMatch& candidate) {
                return candidate.cat_id == cat_id &&
                       candidate.root_node != nullptr;
            });
        if (cat == captured.value.cats.end() ||
            score == ranking.value.ranked.end() ||
            mapped == identity.matches + identity.match_count) {
            continue;
        }
        ++marked;
        std::ostringstream label;
        label << '#' << marked << ' '
              << SafeDisplayName(cat->display_name)
              << ' ' << std::fixed << std::setprecision(1)
              << score->score << " ?";
        labels.push_back(label.str());
        detail_targets.push_back(mapped->component);
    }

    if (marked == 0) {
        recommendation_marker_controller_->CompleteProbe(
            mapping_snapshot_generation_);
        return;
    }
    const auto shown =
        recommendation_marker_controller_->ShowRecommendations(
            mapping_snapshot_generation_,
            labels);
    if (!shown) {
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
    recommendation_detail_targets_ = std::move(detail_targets);
    Logger::Instance().Write(
        LogLevel::Info,
        "RecommendationMarker",
        "AC12106",
        "marked=" + std::to_string(marked) +
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
