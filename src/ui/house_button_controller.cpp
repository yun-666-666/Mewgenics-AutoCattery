#include "auto_cattery/ui/house_button_controller.hpp"

#include <utility>

#include "auto_cattery/logger.hpp"
#include "auto_cattery/workflow/organize_workflow_facade.hpp"

namespace autocattery::ui {
namespace {

constexpr auto kClickDebounce = std::chrono::milliseconds(500);

}  // namespace

HouseButtonController::HouseButtonController(
    HouseButtonView& view,
    workflow::OrganizeWorkflowFacade& workflow,
    Clock clock)
    : view_(view),
      workflow_(workflow),
      clock_(std::move(clock)) {
    if (!clock_) {
        clock_ = [] {
            return std::chrono::steady_clock::now();
        };
    }
}

Result<void> HouseButtonController::Attach(
    const UiContextSnapshot& context) {
    if (context.kind != UiContextKind::House ||
        !context.input_enabled ||
        context.save_in_progress) {
        return {
            ErrorCode::SceneUnavailable,
            "house UI is not safe for interaction"
        };
    }
    if (view_.IsAttached()) {
        return {};
    }

    auto result = view_.Attach(context, [this] {
        HandleClick();
    });
    if (!result) {
        return result;
    }

    scene_generation_ = context.scene_generation;
    SetState(OrganizeButtonState::Ready);
    Logger::Instance().Write(
        LogLevel::Info,
        "HouseButton",
        "AC3100",
        "Attached AutoCattery.House.AutoOrganizeButton.");
    return {};
}

void HouseButtonController::Detach() noexcept {
    if (!view_.IsAttached()) {
        state_ = OrganizeButtonState::Hidden;
        return;
    }
    view_.SetState(OrganizeButtonState::Hidden, {});
    view_.Detach();
    state_ = OrganizeButtonState::Hidden;
    scene_generation_ = 0;
    last_click_ = {};
    Logger::Instance().Write(
        LogLevel::Info,
        "HouseButton",
        "AC3101",
        "Detached AutoCattery.House.AutoOrganizeButton.");
}

void HouseButtonController::SetState(
    OrganizeButtonState state,
    std::string_view detail) {
    state_ = state;
    view_.SetState(state, detail);
}

bool HouseButtonController::IsAttached() const noexcept {
    return view_.IsAttached();
}

void HouseButtonController::HandleClick() {
    if (!view_.IsAttached() ||
        state_ == OrganizeButtonState::Running ||
        state_ == OrganizeButtonState::DisabledBusy ||
        state_ == OrganizeButtonState::DisabledUnsupportedBuild ||
        state_ == OrganizeButtonState::Hidden) {
        return;
    }

    const auto now = clock_();
    if (last_click_.time_since_epoch().count() != 0 &&
        now - last_click_ < kClickDebounce) {
        return;
    }
    last_click_ = now;

    Logger::Instance().Write(
        LogLevel::Info,
        "HouseButton",
        "AC3102",
        "Auto-organize preview clicked; capturing a read-only snapshot.");
    SetState(OrganizeButtonState::Running);
    const auto generation = scene_generation_;
    preview_generation_ = generation;
    preview_task_ = std::async(
        std::launch::async,
        [this, generation] {
            return workflow_.RequestPreview(generation);
        });
}

void HouseButtonController::Poll() {
    if (!preview_task_.valid() ||
        preview_task_.wait_for(std::chrono::milliseconds(0)) !=
            std::future_status::ready) {
        return;
    }
    const auto preview = preview_task_.get();
    if (!view_.IsAttached() ||
        preview_generation_ != scene_generation_) {
        return;
    }
    view_.ShowPlaceholder();
    SetState(
        preview ? OrganizeButtonState::Completed
                : OrganizeButtonState::Failed,
        preview.message);
}

}  // namespace autocattery::ui
