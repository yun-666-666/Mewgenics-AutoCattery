#include "auto_cattery/ui/recommendation_marker_controller.hpp"

#include "auto_cattery/logger.hpp"

namespace autocattery::ui {

RecommendationMarkerController::RecommendationMarkerController(
    RecommendationMarkerView& view)
    : view_(view) {}

Result<void> RecommendationMarkerController::AttachButton(
    const UiContextSnapshot& context) {
    if (context.kind != UiContextKind::House ||
        !context.input_enabled ||
        context.save_in_progress ||
        context.scene_generation == 0) {
        return {
            ErrorCode::SceneUnavailable,
            "the House departure UI is not safe for interaction"
        };
    }

    if (view_.IsAttached() &&
        scene_generation_ == context.scene_generation) {
        return {};
    }
    if (view_.IsAttached()) {
        Detach();
    }

    const auto attached = view_.Attach(context, [this] {
        HandleClick();
    });
    if (!attached) {
        return attached;
    }

    scene_generation_ = context.scene_generation;
    state_ = RecommendationButtonState::Ready;
    view_.SetButtonState(state_);
    Logger::Instance().Write(
        LogLevel::Info,
        "RecommendationMarker",
        "AC4100",
        "Attached the House departure recommendation marker button shell.");
    return {};
}

Result<void> RecommendationMarkerController::ShowDemoMarker(
    void* safe_test_target) {
    if (!view_.IsAttached() ||
        scene_generation_ == 0 ||
        safe_test_target == nullptr) {
        return {
            ErrorCode::UiNodeNotFound,
            "a safe stage-04 test target is unavailable"
        };
    }

    auto shown = view_.ShowDemoTextMarker(scene_generation_);
    if (!shown) {
        return {
            shown.code,
            shown.message
        };
    }
    if (shown.value.target_handle != safe_test_target ||
        shown.value.scene_generation != scene_generation_) {
        view_.ClearAllVisuals();
        return {
            ErrorCode::SceneUnavailable,
            "the demo marker target or scene generation changed"
        };
    }

    demo_visual_ = shown.value;
    return {};
}

void RecommendationMarkerController::ClearAll() noexcept {
    view_.ClearAllVisuals();
    demo_visual_ = {};
    if (view_.IsAttached()) {
        state_ = RecommendationButtonState::Ready;
        view_.SetButtonState(state_);
    }
}

void RecommendationMarkerController::Detach() noexcept {
    if (!view_.IsAttached()) {
        state_ = RecommendationButtonState::Hidden;
        scene_generation_ = 0;
        demo_visual_ = {};
        return;
    }

    // Clear generation-bound visual nodes before releasing their scene.
    ClearAll();
    view_.SetButtonState(RecommendationButtonState::Hidden);
    view_.Detach();
    state_ = RecommendationButtonState::Hidden;
    scene_generation_ = 0;
    Logger::Instance().Write(
        LogLevel::Info,
        "RecommendationMarker",
        "AC4101",
        "Cleared markers and detached the House recommendation button.");
}

void RecommendationMarkerController::HandleClick() {
    if (!view_.IsAttached() ||
        state_ == RecommendationButtonState::Hidden ||
        state_ == RecommendationButtonState::Marking) {
        return;
    }

    if (HasMarkers()) {
        ClearAll();
        Logger::Instance().Write(
            LogLevel::Info,
            "RecommendationMarker",
            "AC4103",
            "Cleared the stage-04 demo recommendation marker.");
        return;
    }

    state_ = RecommendationButtonState::Marking;
    view_.SetButtonState(state_);
    const auto shown = ShowDemoMarker(view_.SafeTestTarget());
    if (!shown) {
        view_.ClearAllVisuals();
        demo_visual_ = {};
        state_ = RecommendationButtonState::Ready;
        view_.SetButtonState(state_);
        Logger::Instance().Write(
            LogLevel::Warn,
            "RecommendationMarker",
            "AC4104",
            "Demo marker was not shown because its safe visual target was unavailable.");
        return;
    }

    state_ = RecommendationButtonState::Marked;
    view_.SetButtonState(state_);
    Logger::Instance().Write(
        LogLevel::Info,
        "RecommendationMarker",
        "AC4102",
        "Displayed one text-only demo marker; no cat or selection data was read or changed.");
}

bool RecommendationMarkerController::IsAttached() const noexcept {
    return view_.IsAttached();
}

bool RecommendationMarkerController::HasMarkers() const noexcept {
    return demo_visual_.target_handle != nullptr &&
           demo_visual_.scene_generation == scene_generation_;
}

std::uint64_t RecommendationMarkerController::SceneGeneration() const noexcept {
    return scene_generation_;
}

}  // namespace autocattery::ui
