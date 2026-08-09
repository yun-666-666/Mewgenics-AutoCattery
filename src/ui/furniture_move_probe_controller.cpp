#include "furniture_move_probe_controller.hpp"

#include "auto_cattery/save_safety/game_build_gate.hpp"
#include "furniture_move_probe_report.hpp"

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
    before_furniture_ui_.reset();
    before_house_scene_.reset();
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
        before_furniture_ui_ = std::move(before_ui);
        before_house_scene_ = std::move(before_house);
        return {FurnitureMoveProbeEventKind::BeforeCaptured,
            "F7 captured the furniture pre-move object graph; move and place one furniture item, then press F7 again"};
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

    auto report = std::make_unique<FurnitureMoveProbeReport>();
    report->scene_generation = context.scene_generation;
    AcMewCompareFurnitureMoveSamples(
        before_furniture_ui_.get(),
        after_ui.get(),
        AC_MEW_FURNITURE_PROBE_UI,
        &report->furniture_ui);
    AcMewCompareFurnitureMoveSamples(
        before_house_scene_.get(),
        after_house.get(),
        AC_MEW_FURNITURE_PROBE_HOUSE_SCENE,
        &report->house_scene);
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
