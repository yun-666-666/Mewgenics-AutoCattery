#include "auto_cattery/ui/house_button_controller.hpp"

#include <utility>

#include "auto_cattery/logger.hpp"
#include "auto_cattery/workflow/organize_workflow_facade.hpp"

namespace autocattery::ui {
namespace {

constexpr auto kClickDebounce = std::chrono::milliseconds(500);
constexpr auto kFailureHold = std::chrono::milliseconds(500);

}  // namespace

HouseButtonController::HouseButtonController(
    HouseButtonView& view,
    workflow::OrganizeWorkflowFacade& workflow,
    Clock clock,
    BeforePreview before_preview)
    : view_(view),
      workflow_(workflow),
      clock_(std::move(clock)),
      before_preview_(std::move(before_preview)) {
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
    continuation_preview_pending_ = false;
    continuation_preview_ = false;
    continuation_execute_pending_ = false;
    awaiting_execution_ = false;
    ready_after_ = {};
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
        state_detail_.clear();
        return;
    }
    view_.SetState(OrganizeButtonState::Hidden, {});
    view_.Detach();
    state_ = OrganizeButtonState::Hidden;
    state_detail_.clear();
    scene_generation_ = 0;
    continuation_preview_pending_ = false;
    continuation_preview_ = false;
    continuation_execute_pending_ = false;
    awaiting_execution_ = false;
    last_click_ = {};
    ready_after_ = {};
    Logger::Instance().Write(
        LogLevel::Info,
        "HouseButton",
        "AC3101",
        "Detached AutoCattery.House.AutoOrganizeButton.");
}

void HouseButtonController::AbandonScene() noexcept {
    view_.AbandonScene();
    state_ = OrganizeButtonState::Hidden;
    state_detail_.clear();
    suppressed_ = false;
    scene_generation_ = 0;
    continuation_preview_pending_ = false;
    continuation_preview_ = false;
    continuation_execute_pending_ = false;
    awaiting_execution_ = false;
    last_click_ = {};
    ready_after_ = {};
}

void HouseButtonController::SetSuppressed(bool suppressed) {
    if (suppressed_ == suppressed) return;
    suppressed_ = suppressed;
    if (!view_.IsAttached()) return;
    if (suppressed_) {
        view_.SetState(OrganizeButtonState::Hidden, {});
        return;
    }
    view_.SetState(state_, state_detail_);
}

void HouseButtonController::SetFurnitureMode(bool furniture_mode) {
    if (furniture_mode_ == furniture_mode) return;
    furniture_mode_ = furniture_mode;
    view_.SetFurnitureMode(furniture_mode_);
}

void HouseButtonController::SetState(
    OrganizeButtonState state,
    std::string_view detail) {
    state_ = state;
    state_detail_ = detail;
    view_.SetState(
        suppressed_ ? OrganizeButtonState::Hidden : state,
        suppressed_ ? std::string_view{} : std::string_view{state_detail_});
}

bool HouseButtonController::IsAttached() const noexcept {
    return view_.IsAttached();
}

bool HouseButtonController::IsSuppressed() const noexcept {
    return suppressed_;
}

void HouseButtonController::HandleClick() {
    if (!view_.IsAttached() ||
        suppressed_ ||
        furniture_mode_ ||
        state_ != OrganizeButtonState::Ready) {
        return;
    }

    const auto now = clock_();
    if (last_click_.time_since_epoch().count() != 0 &&
        now - last_click_ < kClickDebounce) {
        return;
    }
    last_click_ = now;

    if (awaiting_execution_) {
        Logger::Instance().Write(
            LogLevel::Info,
            "HouseButton",
            "AC3104",
            "Move-only execution confirmed; applying the preview through "
            "the native House room path.");
        SetState(OrganizeButtonState::Running);
        ExecuteBatch();
        return;
    }

    Logger::Instance().Write(
        LogLevel::Info,
        "HouseButton",
        "AC3102",
        "Auto-organize preview clicked; capturing a read-only snapshot.");
    StartPreview(false);
}

void HouseButtonController::Poll() {
    if (continuation_preview_pending_) {
        continuation_preview_pending_ = false;
        StartPreview(true);
        return;
    }
    if (continuation_execute_pending_) {
        continuation_execute_pending_ = false;
        ExecuteBatch();
        return;
    }
    if (preview_task_.valid() &&
        preview_task_.wait_for(std::chrono::milliseconds(0)) ==
            std::future_status::ready) {
        const bool continuation = continuation_preview_;
        continuation_preview_ = false;
        const auto preview = preview_task_.get();
        if (!view_.IsAttached() ||
            preview_generation_ != scene_generation_) {
            continuation_execute_pending_ = false;
            return;
        }
        if (continuation) {
            if (!preview) {
                awaiting_execution_ = false;
                SetState(OrganizeButtonState::Failed, preview.message);
                ready_after_ = clock_() + kFailureHold;
                return;
            }
            SetState(OrganizeButtonState::Running, preview.message);
            continuation_execute_pending_ = true;
            return;
        }
        view_.ShowPlaceholder();
        SetState(
            preview ? OrganizeButtonState::Completed
                    : OrganizeButtonState::Failed,
            preview.message);
        awaiting_execution_ =
            preview &&
            workflow_.CurrentExecutionAvailability() !=
                workflow::WorkflowCapability::PreviewOnly;
        if (awaiting_execution_) {
            last_click_ = {};
            ready_after_ = {};
            SetState(OrganizeButtonState::Ready, preview.message);
        } else {
            ready_after_ = clock_() + kFailureHold;
        }
    }

    if (ready_after_.time_since_epoch().count() != 0 &&
        clock_() >= ready_after_) {
        ready_after_ = {};
        if (view_.IsAttached()) {
            SetState(OrganizeButtonState::Ready);
        }
    }
}

void HouseButtonController::StartPreview(bool continuation) {
    if (before_preview_) {
        before_preview_();
    }
    SetState(OrganizeButtonState::Running);
    const auto generation = scene_generation_;
    preview_generation_ = generation;
    continuation_preview_ = continuation;
    preview_task_ = std::async(
        std::launch::async,
        [this, generation] {
            return workflow_.RequestPreview(generation);
        });
}

void HouseButtonController::ExecuteBatch() {
    const auto execution = workflow_.RequestExecutionOutcome();
    awaiting_execution_ = false;
    if (!execution) {
        continuation_preview_pending_ = false;
        continuation_preview_ = false;
        continuation_execute_pending_ = false;
        SetState(OrganizeButtonState::Failed, execution.message);
        ready_after_ = clock_() + kFailureHold;
        return;
    }
    if (execution.value.remaining_moves != 0) {
        SetState(OrganizeButtonState::Running, execution.message);
        continuation_preview_pending_ = true;
        return;
    }
    last_click_ = {};
    SetState(OrganizeButtonState::Ready, execution.message);
}

}  // namespace autocattery::ui
