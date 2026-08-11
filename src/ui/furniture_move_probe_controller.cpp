#include "furniture_move_probe_controller.hpp"

#include "auto_cattery/save_safety/game_build_gate.hpp"
#include "furniture_move_probe_report.hpp"
#include "mew_ui_scene_probe.h"

#include <algorithm>
#include <unordered_map>

namespace {

constexpr std::size_t kFurniturePieceCapacity = 512U;

std::unique_ptr<AcMewFurnitureMoveSample> CaptureOptional(void* root) {
    if (!root) {
        return {};
    }
    auto sample = std::make_unique<AcMewFurnitureMoveSample>();
    if (!AcMewCaptureFurnitureMoveSample(root, sample.get())) {
        return {};
    }
    return sample;
}

std::vector<AcMewFurniturePieceSnapshot> CaptureFurniturePieces(
    void* house_scene_manager,
    bool& complete) {
    std::vector<AcMewFurniturePieceSnapshot> pieces(
        kFurniturePieceCapacity);
    std::uint8_t native_complete{};
    const auto count = AcMewEnumerateFurniturePieces(
        house_scene_manager,
        pieces.data(),
        pieces.size(),
        &native_complete);
    pieces.resize(count);
    complete = native_complete != 0U;
    return pieces;
}

autocattery::ui::FurnitureProbePiece Evidence(
    const AcMewFurniturePieceSnapshot& piece) {
    return {
        .stable_key = piece.stable_key,
        .item = piece.item,
        .room = piece.room,
        .saved_x = piece.saved_x,
        .saved_y = piece.saved_y,
        .grid_present = piece.grid != nullptr
    };
}

bool PlacementChanged(
    const AcMewFurniturePieceSnapshot& before,
    const AcMewFurniturePieceSnapshot& after) {
    return before.grid != after.grid ||
        before.saved_x != after.saved_x ||
        before.saved_y != after.saved_y ||
        std::string_view(before.room) != std::string_view(after.room);
}

autocattery::ui::FurnitureProbeSceneDelta BuildSceneDelta(
    const std::vector<AcMewFurniturePieceSnapshot>& before,
    bool before_complete,
    const std::vector<AcMewFurniturePieceSnapshot>& after,
    bool after_complete) {
    autocattery::ui::FurnitureProbeSceneDelta result{
        .before_count = before.size(),
        .after_count = after.size(),
        .before_complete = before_complete,
        .after_complete = after_complete
    };
    std::unordered_map<std::uint64_t, const AcMewFurniturePieceSnapshot*>
        before_by_key;
    std::unordered_map<std::uint64_t, const AcMewFurniturePieceSnapshot*>
        after_by_key;
    for (const auto& piece : before) {
        if (piece.stable_key != 0U) {
            before_by_key.emplace(piece.stable_key, &piece);
        }
    }
    for (const auto& piece : after) {
        if (piece.stable_key != 0U) {
            after_by_key.emplace(piece.stable_key, &piece);
        }
    }
    for (const auto& [key, piece] : before_by_key) {
        const auto found = after_by_key.find(key);
        if (found == after_by_key.end()) {
            result.disappeared.push_back(Evidence(*piece));
        } else if (PlacementChanged(*piece, *found->second)) {
            result.changed.push_back(Evidence(*found->second));
        }
    }
    for (const auto& [key, piece] : after_by_key) {
        if (!before_by_key.contains(key)) {
            result.appeared.push_back(Evidence(*piece));
        }
    }
    const auto by_key = [](const auto& left, const auto& right) {
        return left.stable_key < right.stable_key;
    };
    std::ranges::sort(result.appeared, by_key);
    std::ranges::sort(result.disappeared, by_key);
    std::ranges::sort(result.changed, by_key);
    return result;
}

}  // namespace

namespace autocattery::ui {

void FurnitureMoveProbeController::Initialize(
    const std::filesystem::path& game_executable,
    std::filesystem::path diagnostics_root) {
    Shutdown();
    diagnostics_root_ = std::move(diagnostics_root);
    build_supported_ = static_cast<bool>(
        save_safety::CurrentMewgenicsBuildGate().Verify(game_executable));
}

void FurnitureMoveProbeController::Shutdown() noexcept {
    ClearCapture();
    diagnostics_root_.clear();
    build_supported_ = false;
}

void FurnitureMoveProbeController::ClearCapture() noexcept {
    scene_generation_ = 0;
    house_scene_manager_ = nullptr;
    furniture_ui_ = nullptr;
    house_inventory_ = nullptr;
    furniture_editor_ = nullptr;
    furniture_click_handler_ = nullptr;
    before_furniture_ui_.reset();
    before_house_inventory_.reset();
    before_furniture_editor_.reset();
    before_furniture_click_handler_.reset();
    before_house_scene_.reset();
    before_furniture_pieces_.clear();
    before_furniture_pieces_complete_ = false;
}

bool FurnitureMoveProbeController::SafeFurnitureMode(
    const UiContextSnapshot& context,
    void* house_scene_manager,
    bool furniture_mode,
    void* furniture_ui) {
    return context.kind == UiContextKind::House &&
        context.input_enabled &&
        !context.save_in_progress &&
        context.scene_generation != 0 &&
        house_scene_manager != nullptr &&
        furniture_mode &&
        furniture_ui != nullptr;
}

FurnitureMoveProbeEvent FurnitureMoveProbeController::Poll(
    const UiContextSnapshot& context,
    void* house_scene_manager,
    bool furniture_mode,
    void* furniture_ui,
    bool capture_pressed) {
    const bool has_before = before_furniture_ui_ && before_house_scene_;
    if (has_before &&
        (!SafeFurnitureMode(
             context,
             house_scene_manager,
             furniture_mode,
             furniture_ui) ||
         context.scene_generation != scene_generation_ ||
         house_scene_manager != house_scene_manager_ ||
         furniture_ui != furniture_ui_)) {
        ClearCapture();
        return {FurnitureMoveProbeEventKind::Cancelled,
            "F7 furniture move probe cancelled because the furniture scene changed"};
    }
    if (!capture_pressed) {
        return {};
    }
    if (!build_supported_) {
        return {FurnitureMoveProbeEventKind::Rejected,
            "F7 furniture move probe rejected: Mewgenics.exe is not the verified build"};
    }
    if (!SafeFurnitureMode(
            context,
            house_scene_manager,
            furniture_mode,
            furniture_ui)) {
        return {FurnitureMoveProbeEventKind::Rejected,
            "F7 furniture move probe requires the active furniture placement screen"};
    }
    if (!has_before) {
        auto before_ui = std::make_unique<AcMewFurnitureMoveSample>();
        auto before_house = std::make_unique<AcMewFurnitureMoveSample>();
        if (!AcMewCaptureFurnitureMoveSample(
                furniture_ui,
                before_ui.get()) ||
            !AcMewCaptureFurnitureMoveSample(
                house_scene_manager,
                before_house.get())) {
            return {FurnitureMoveProbeEventKind::Rejected,
                "F7 furniture move probe could not capture the pre-move object graph"};
        }
        scene_generation_ = context.scene_generation;
        house_scene_manager_ = house_scene_manager;
        furniture_ui_ = furniture_ui;
        house_inventory_ = AcMewFindComponentByType(
            house_scene_manager, "HouseInventory");
        furniture_editor_ = AcMewFindComponentByType(
            house_scene_manager, "FurnitureEditor");
        furniture_click_handler_ = AcMewFindComponentByType(
            house_scene_manager, "FurnitureClickHandler");
        before_furniture_ui_ = std::move(before_ui);
        before_house_inventory_ = CaptureOptional(house_inventory_);
        before_furniture_editor_ = CaptureOptional(furniture_editor_);
        before_furniture_click_handler_ =
            CaptureOptional(furniture_click_handler_);
        before_house_scene_ = std::move(before_house);
        before_furniture_pieces_ = CaptureFurniturePieces(
            house_scene_manager,
            before_furniture_pieces_complete_);
        return {FurnitureMoveProbeEventKind::BeforeCaptured,
            "F7 captured the warehouse pre-take state; manually take one furniture item from the drawer, place it in a room, then press F7 again"};
    }

    auto after_ui = std::make_unique<AcMewFurnitureMoveSample>();
    auto after_house = std::make_unique<AcMewFurnitureMoveSample>();
    if (!AcMewCaptureFurnitureMoveSample(furniture_ui, after_ui.get()) ||
        !AcMewCaptureFurnitureMoveSample(
            house_scene_manager,
            after_house.get())) {
        ClearCapture();
        return {FurnitureMoveProbeEventKind::Rejected,
            "F7 furniture move probe could not capture the post-move object graph"};
    }
    const auto after_house_inventory = CaptureOptional(
        AcMewFindComponentByType(house_scene_manager, "HouseInventory"));
    const auto after_furniture_editor = CaptureOptional(
        AcMewFindComponentByType(house_scene_manager, "FurnitureEditor"));
    const auto after_furniture_click_handler = CaptureOptional(
        AcMewFindComponentByType(
            house_scene_manager, "FurnitureClickHandler"));
    bool after_furniture_pieces_complete{};
    const auto after_furniture_pieces = CaptureFurniturePieces(
        house_scene_manager,
        after_furniture_pieces_complete);

    auto report = std::make_unique<FurnitureMoveProbeReport>();
    report->scene_generation = context.scene_generation;
    AcMewCompareFurnitureMoveSamples(
        before_furniture_ui_.get(),
        after_ui.get(),
        AC_MEW_FURNITURE_PROBE_UI,
        &report->furniture_ui);
    if (before_house_inventory_ && after_house_inventory) {
        AcMewCompareFurnitureMoveSamples(
            before_house_inventory_.get(),
            after_house_inventory.get(),
            AC_MEW_FURNITURE_PROBE_HOUSE_INVENTORY,
            &report->house_inventory);
    }
    if (before_furniture_editor_ && after_furniture_editor) {
        AcMewCompareFurnitureMoveSamples(
            before_furniture_editor_.get(),
            after_furniture_editor.get(),
            AC_MEW_FURNITURE_PROBE_EDITOR,
            &report->furniture_editor);
    }
    if (before_furniture_click_handler_ &&
        after_furniture_click_handler) {
        AcMewCompareFurnitureMoveSamples(
            before_furniture_click_handler_.get(),
            after_furniture_click_handler.get(),
            AC_MEW_FURNITURE_PROBE_CLICK_HANDLER,
            &report->furniture_click_handler);
    }
    AcMewCompareFurnitureMoveSamples(
        before_house_scene_.get(),
        after_house.get(),
        AC_MEW_FURNITURE_PROBE_HOUSE_SCENE,
        &report->house_scene);
    report->scene_furniture = BuildSceneDelta(
        before_furniture_pieces_,
        before_furniture_pieces_complete_,
        after_furniture_pieces,
        after_furniture_pieces_complete);
    auto written = WriteFurnitureMoveProbeReport(
        diagnostics_root_,
        *report);
    ClearCapture();
    if (!written) {
        return {FurnitureMoveProbeEventKind::Rejected,
            "F7 furniture move probe report failed: " + written.message};
    }
    return {
        FurnitureMoveProbeEventKind::ReportWritten,
        "F7 furniture move probe report written: " +
            written.value.filename().string(),
        written.value.filename().string()};
}

}  // namespace autocattery::ui
