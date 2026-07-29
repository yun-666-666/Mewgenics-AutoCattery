#include "auto_cattery/ui/settings_panel_controller.hpp"

#include <algorithm>

namespace autocattery::ui {
namespace {

constexpr std::size_t kVisibleRows = 4;

}  // namespace

SettingsPanelController::SettingsPanelController(
    SettingsService& settings,
    SettingsPanelView& view)
    : settings_(settings),
      view_(view) {}

Result<void> SettingsPanelController::Attach(
    const UiContextSnapshot& context) {
    if (open_) {
        return Refresh();
    }
    const auto attached = view_.Attach(context);
    if (!attached) {
        return attached;
    }
    open_ = true;
    page_index_ = 0;
    selected_index_ = 0;
    single_click_confirmation_pending_ = false;
    status_.clear();
    return RenderCurrent();
}

void SettingsPanelController::Detach() noexcept {
    view_.Detach();
    open_ = false;
    page_index_ = 0;
    selected_index_ = 0;
    single_click_confirmation_pending_ = false;
    status_.clear();
}

Result<void> SettingsPanelController::Refresh() {
    if (!open_ || !view_.IsAttached()) {
        return {ErrorCode::SceneUnavailable, "settings panel is not open"};
    }
    return RenderCurrent();
}

Result<void> SettingsPanelController::NextPage(int direction) {
    const auto pages = settings_.Pages();
    if (!open_ || pages.empty()) {
        return {ErrorCode::SceneUnavailable, "settings panel is not open"};
    }
    const auto page_count = pages.size();
    if (direction < 0) {
        page_index_ = page_index_ == 0 ? page_count - 1 : page_index_ - 1;
    } else {
        page_index_ = (page_index_ + 1) % page_count;
    }
    selected_index_ = 0;
    single_click_confirmation_pending_ = false;
    status_.clear();
    return RenderCurrent();
}

Result<void> SettingsPanelController::MoveSelection(int direction) {
    const auto pages = settings_.Pages();
    if (!open_ || page_index_ >= pages.size() ||
        pages[page_index_].controls.empty()) {
        return {ErrorCode::SceneUnavailable, "settings page is unavailable"};
    }
    const auto count = pages[page_index_].controls.size();
    if (direction < 0) {
        selected_index_ = selected_index_ == 0 ? count - 1 : selected_index_ - 1;
    } else {
        selected_index_ = (selected_index_ + 1) % count;
    }
    single_click_confirmation_pending_ = false;
    status_.clear();
    return RenderCurrent();
}

Result<void> SettingsPanelController::SelectVisibleRow(std::size_t row) {
    const auto pages = settings_.Pages();
    if (!open_ || page_index_ >= pages.size()) {
        return {ErrorCode::SceneUnavailable, "settings page is unavailable"};
    }
    const auto& controls = pages[page_index_].controls;
    const auto first = (selected_index_ / kVisibleRows) * kVisibleRows;
    if (row >= kVisibleRows || first + row >= controls.size()) {
        return {ErrorCode::ConfigInvalid, "settings row is unavailable"};
    }
    selected_index_ = first + row;
    single_click_confirmation_pending_ = false;
    status_.clear();
    return RenderCurrent();
}

Result<void> SettingsPanelController::AdjustSelected(
    int direction,
    workflow::WorkflowState workflow_state) {
    if (direction == 0) {
        return {};
    }
    return ApplySelected(direction, false, workflow_state);
}

Result<void> SettingsPanelController::ActivateSelected(
    workflow::WorkflowState workflow_state) {
    return ApplySelected(1, true, workflow_state);
}

bool SettingsPanelController::IsOpen() const noexcept {
    return open_;
}

std::size_t SettingsPanelController::PageIndex() const noexcept {
    return page_index_;
}

std::size_t SettingsPanelController::SelectedIndex() const noexcept {
    return selected_index_;
}

Result<void> SettingsPanelController::ApplySelected(
    int direction,
    bool toggle,
    workflow::WorkflowState workflow_state) {
    const auto pages = settings_.Pages();
    if (!open_ || page_index_ >= pages.size() ||
        selected_index_ >= pages[page_index_].controls.size()) {
        return {ErrorCode::SceneUnavailable, "settings control is unavailable"};
    }
    const auto& control = pages[page_index_].controls[selected_index_];
    if (control.read_only) {
        status_ = "Locked";
        return RenderCurrent();
    }

    bool confirmed{};
    if (control.key == "execution_safety.single_click_execute") {
        const auto config = settings_.Current();
        const bool enable = toggle
            ? !config.execution_safety.single_click_execute
            : direction > 0;
        if (enable && !single_click_confirmation_pending_) {
            single_click_confirmation_pending_ = true;
            status_ = "Confirm";
            return RenderCurrent();
        }
        confirmed = enable && single_click_confirmation_pending_;
        single_click_confirmation_pending_ = false;
    }

    const auto applied = settings_.AdjustControl(
        control.key,
        direction,
        toggle,
        confirmed,
        workflow_state);

    RecordApplyResult(applied);
    const auto rendered = RenderCurrent();
    if (!rendered) {
        return rendered;
    }
    return applied;
}

Result<void> SettingsPanelController::RenderCurrent() {
    const auto pages = settings_.Pages();
    if (!open_ || page_index_ >= pages.size()) {
        return {ErrorCode::SceneUnavailable, "settings page is unavailable"};
    }
    const auto& page = pages[page_index_];
    if (page.controls.empty()) {
        return {ErrorCode::ConfigInvalid, "settings page has no controls"};
    }
    selected_index_ = std::min(selected_index_, page.controls.size() - 1);
    const auto first = (selected_index_ / kVisibleRows) * kVisibleRows;
    const auto count = std::min(kVisibleRows, page.controls.size() - first);
    auto title = page.title;
    if (!status_.empty()) {
        title += " " + status_;
    }
    return view_.Render(
        title,
        std::span<const SettingsControl>(page.controls).subspan(first, count),
        selected_index_ - first);
}

void SettingsPanelController::RecordApplyResult(const Result<void>& result) {
    if (result) {
        status_ = "Applied";
    } else if (result.code == ErrorCode::WriteConflict) {
        status_ = "Pending";
    } else {
        status_ = "Rejected";
    }
}

}  // namespace autocattery::ui
