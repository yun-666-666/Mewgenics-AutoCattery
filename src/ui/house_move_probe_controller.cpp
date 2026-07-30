#include "house_move_probe_controller.hpp"

#include "auto_cattery/save_safety/game_build_gate.hpp"

namespace autocattery::ui {

void HouseMoveProbeController::Initialize(
    const std::filesystem::path& game_executable,
    std::filesystem::path diagnostics_root) {
    Shutdown();
    diagnostics_root_ = std::move(diagnostics_root);
    build_supported_ = static_cast<bool>(
        save_safety::CurrentMewgenicsBuildGate().Verify(game_executable));
}

void HouseMoveProbeController::Shutdown() noexcept {
    enabled_ = false;
    previous_left_down_ = false;
    scene_generation_ = 0;
    mapper_.Cancel();
    session_.Cancel();
    diagnostics_root_.clear();
    build_supported_ = false;
}

bool HouseMoveProbeController::SafeHouse(
    const UiContextSnapshot& context,
    void* house_scene_manager) {
    return house_scene_manager != nullptr &&
           context.kind == UiContextKind::House &&
           context.input_enabled &&
           !context.save_in_progress &&
           context.scene_generation != 0;
}

HouseMoveProbeEvent HouseMoveProbeController::Disable(
    std::string message) {
    enabled_ = false;
    previous_left_down_ = false;
    scene_generation_ = 0;
    mapper_.Cancel();
    session_.Cancel();
    return {HouseMoveProbeEventKind::Disabled, std::move(message)};
}

HouseMoveProbeEvent HouseMoveProbeController::Poll(
    const UiContextSnapshot& context,
    void* house_scene_manager,
    bool toggle_pressed,
    bool left_mouse_down) {
    if (!enabled_ && mapper_.Loading()) {
        (void)mapper_.Poll(context.scene_generation, nullptr);
    }
    if (toggle_pressed) {
        if (enabled_) {
            return Disable("F9 read-only move probe disabled");
        }
        if (!build_supported_) {
            return {HouseMoveProbeEventKind::Rejected,
                "F9 probe rejected: current game build is not an exact match"};
        }
        if (mapper_.Loading()) {
            return {HouseMoveProbeEventKind::Rejected,
                "F9 probe mapping task is still stopping"};
        }
        if (!SafeHouse(context, house_scene_manager)) {
            return {HouseMoveProbeEventKind::Rejected,
                "F9 probe requires a stable, input-enabled House scene"};
        }
        enabled_ = true;
        scene_generation_ = context.scene_generation;
        previous_left_down_ = left_mouse_down;
        mapper_.Start(scene_generation_);
        return {HouseMoveProbeEventKind::Loading,
            "F9 read-only move probe loading exact HouseCat mapping"};
    }
    if (!enabled_) {
        return {};
    }
    if (!SafeHouse(context, house_scene_manager) ||
        context.scene_generation != scene_generation_) {
        return Disable(
            "F9 probe stopped because the House scene became unsafe");
    }

    if (!session_.Armed()) {
        auto mapping = mapper_.Poll(
            context.scene_generation,
            house_scene_manager);
        if (mapping.status == HouseMoveMappingStatus::Loading ||
            mapping.status == HouseMoveMappingStatus::Idle) {
            previous_left_down_ = left_mouse_down;
            return {};
        }
        if (mapping.status == HouseMoveMappingStatus::Rejected) {
            enabled_ = false;
            scene_generation_ = 0;
            return {HouseMoveProbeEventKind::Rejected,
                "F9 probe mapping rejected: " + mapping.message};
        }
        session_.Arm(scene_generation_, std::move(mapping.matches));
        previous_left_down_ = left_mouse_down;
        return {HouseMoveProbeEventKind::Ready,
            "F9 probe ready; drag one cat between rooms"};
    }

    if (left_mouse_down && !previous_left_down_) {
        previous_left_down_ = true;
        if (!session_.CaptureBefore()) {
            return Disable(
                "F9 probe stopped because the before sample was incomplete");
        }
        return {HouseMoveProbeEventKind::BeforeCaptured,
            "F9 probe captured the pre-drag sample"};
    }
    if (!left_mouse_down && previous_left_down_) {
        previous_left_down_ = false;
        if (!session_.HasBeforeSample()) {
            return {};
        }
        auto report = session_.CaptureAfter();
        if (!report) {
            return Disable("F9 probe stopped: " + report.message);
        }
        auto written = WriteHouseMoveProbeReport(
            diagnostics_root_,
            report.value);
        if (!written) {
            return Disable("F9 probe report failed: " + written.message);
        }
        return {HouseMoveProbeEventKind::ReportWritten,
            "F9 probe report written: " +
                written.value.filename().string()};
    }
    return {};
}

}  // namespace autocattery::ui
