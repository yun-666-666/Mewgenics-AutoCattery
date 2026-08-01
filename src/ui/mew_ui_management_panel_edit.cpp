#include "mew_ui_management_panel_view.hpp"

#include <utility>

namespace autocattery::ui {

void MewUiManagementPanelView::BeginNumericInput(
    std::size_t row, std::string value) {
    edit_buffer_ = std::move(value);
    editing_row_.store(static_cast<int>(row));
    edit_replace_on_type_.store(true);
}

void MewUiManagementPanelView::CancelNumericInput() noexcept {
    editing_row_.store(-1);
    edit_replace_on_type_.store(false);
    edit_buffer_.clear();
}

bool MewUiManagementPanelView::IsEditing() const noexcept {
    return editing_row_.load() >= 0;
}

bool MewUiManagementPanelView::CaptureEditMessage(MSG* message) noexcept {
    const int row = editing_row_.load();
    if (message == nullptr || message->message != WM_CHAR || row < 0)
        return false;
    const auto character = static_cast<char>(message->wParam);
    auto control = ManagementPanelControl::EditChanged;
    bool changed = false;
    if (character == '\r') {
        control = ManagementPanelControl::CommitEdit;
        changed = true;
    } else if (character == '\b') {
        if (edit_replace_on_type_.exchange(false)) edit_buffer_.clear();
        if (!edit_buffer_.empty()) edit_buffer_.pop_back();
        changed = true;
    } else if ((character >= '0' && character <= '9') ||
               character == '.' || character == '-' || character == '+') {
        if (edit_replace_on_type_.exchange(false)) edit_buffer_.clear();
        if (edit_buffer_.size() < 24) edit_buffer_.push_back(character);
        changed = true;
    }
    if (changed) {
        pending_control_.store(static_cast<int>(control));
        pending_row_.store(row);
        pending_direction_.store(0);
    }
    message->message = WM_NULL;
    return changed;
}

}  // namespace autocattery::ui
