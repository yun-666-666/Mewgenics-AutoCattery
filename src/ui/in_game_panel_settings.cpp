#include "in_game_panel_controller.hpp"

namespace autocattery::ui {

void InGamePanelController::HandleSettingsEvent(
    const ManagementPanelEvent& event) {
    if (event.control == ManagementPanelControl::Row) {
        if (view_.IsEditing()) {
            view_.CancelNumericInput();
            editing_setting_.reset();
            editing_text_.clear();
        }
        const auto adjusted = settings_.AdjustFlat(event.row, event.direction);
        status_ = adjusted ? "已保存；游戏中的规则会自动热更新"
                           : "保存失败：" + adjusted.message;
        return;
    }
    if (event.control == ManagementPanelControl::BeginEdit) {
        const auto value = settings_.DirectValue(event.row);
        if (!value) {
            const auto adjusted = settings_.AdjustFlat(event.row, 0);
            status_ = adjusted ? "已切换并保存"
                               : "保存失败：" + adjusted.message;
            return;
        }
        editing_setting_ = event.row;
        editing_text_ = *value;
        view_.BeginNumericInput(event.row, *value);
        status_ = "直接输入新数值后按 Enter，按 Esc 取消；当前值：" +
            *value;
        return;
    }
    if (event.control == ManagementPanelControl::EditChanged &&
        editing_setting_ == event.row) {
        editing_text_ = event.text;
        status_ = "正在输入：" + event.text +
            "（Enter 保存，Esc 取消）";
        return;
    }
    if (event.control != ManagementPanelControl::CommitEdit ||
        editing_setting_ != event.row) return;
    const auto saved = settings_.SetFlatValue(event.row, event.text);
    editing_text_ = event.text;
    status_ = saved ? "数值已保存；游戏中的规则会自动热更新"
                    : "输入无效：" + saved.message;
    if (saved) {
        view_.CancelNumericInput();
        editing_setting_.reset();
        editing_text_.clear();
    }
}

}  // namespace autocattery::ui
