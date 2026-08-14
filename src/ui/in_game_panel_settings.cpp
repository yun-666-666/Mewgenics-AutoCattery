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
        const bool restart = settings_.RequiresGameRestart(event.row);
        const auto adjusted = settings_.AdjustFlat(event.row, event.direction);
        status_ = adjusted
            ? (restart
                   ? (English() ? "Saved; restart the game to apply rerolls"
                                : "已保存；重启游戏后应用重骰次数")
                   : (English() ? "Saved; game rules will hot-reload"
                                : "已保存；游戏中的规则会自动热更新"))
            : (English() ? "Save failed: " : "保存失败：") +
                adjusted.message;
        return;
    }
    if (event.control == ManagementPanelControl::BeginEdit) {
        const auto value = settings_.DirectValue(event.row);
        if (!value) {
            const auto adjusted = settings_.AdjustFlat(event.row, 0);
            status_ = adjusted
                ? (English() ? "Switched and saved" : "已切换并保存")
                : (English() ? "Save failed: " : "保存失败：") +
                    adjusted.message;
            return;
        }
        editing_setting_ = event.row;
        editing_text_ = *value;
        view_.BeginNumericInput(event.row, *value);
        status_ = (English()
            ? "Type a new value, Enter to save, Esc to cancel. Current: "
            : "直接输入新数值后按 Enter，按 Esc 取消；当前值：") + *value;
        return;
    }
    if (event.control == ManagementPanelControl::EditChanged &&
        editing_setting_ == event.row) {
        editing_text_ = event.text;
        status_ = (English() ? "Editing: " : "正在输入：") + event.text +
            (English() ? " (Enter saves, Esc cancels)"
                       : "（Enter 保存，Esc 取消）");
        return;
    }
    if (event.control != ManagementPanelControl::CommitEdit ||
        editing_setting_ != event.row) return;
    const bool restart = settings_.RequiresGameRestart(event.row);
    const auto saved = settings_.SetFlatValue(event.row, event.text);
    editing_text_ = event.text;
    status_ = saved
        ? (restart
               ? (English() ? "Saved; restart the game to apply rerolls"
                            : "已保存；重启游戏后应用重骰次数")
               : (English() ? "Value saved; game rules will hot-reload"
                            : "数值已保存；游戏中的规则会自动热更新"))
        : (English() ? "Invalid input: " : "输入无效：") + saved.message;
    if (saved) {
        view_.CancelNumericInput();
        editing_setting_.reset();
        editing_text_.clear();
    }
}

void InGamePanelController::HandleFurnitureEvent(
    const ManagementPanelEvent& event) {
    if (event.control == ManagementPanelControl::Row) {
        if (view_.IsEditing()) {
            view_.CancelNumericInput();
            editing_setting_.reset();
            editing_text_.clear();
        }
        const auto adjusted = settings_.AdjustFurniture(
            event.row, event.direction);
        status_ = adjusted
            ? (English() ? "Saved; auto-placement rules will hot-reload"
                         : "已保存；自动放置规则会热更新")
            : (English() ? "Save failed: " : "保存失败：") +
                adjusted.message;
        return;
    }
    if (event.control == ManagementPanelControl::BeginEdit) {
        const auto value = settings_.DirectFurnitureValue(event.row);
        if (!value) {
            const auto adjusted = settings_.AdjustFurniture(event.row, 0);
            status_ = adjusted
                ? (English() ? "Switched and saved" : "已切换并保存")
                : (English() ? "Save failed: " : "保存失败：") +
                    adjusted.message;
            return;
        }
        editing_setting_ = event.row;
        editing_text_ = *value;
        view_.BeginNumericInput(event.row, *value);
        status_ = (English()
            ? "Type a new value, Enter to save, Esc to cancel. Current: "
            : "直接输入新数值后按 Enter，按 Esc 取消；当前值：") + *value;
        return;
    }
    if (event.control == ManagementPanelControl::EditChanged &&
        editing_setting_ == event.row) {
        editing_text_ = event.text;
        status_ = (English() ? "Editing: " : "正在输入：") + event.text +
            (English() ? " (Enter saves, Esc cancels)"
                       : "（Enter 保存，Esc 取消）");
        return;
    }
    if (event.control != ManagementPanelControl::CommitEdit ||
        editing_setting_ != event.row) return;
    const auto saved = settings_.SetFurnitureValue(event.row, event.text);
    editing_text_ = event.text;
    status_ = saved
        ? (English() ? "Value saved; auto-placement rules will hot-reload"
                     : "数值已保存；自动放置规则会热更新")
        : (English() ? "Invalid input: " : "输入无效：") + saved.message;
    if (saved) {
        view_.CancelNumericInput();
        editing_setting_.reset();
        editing_text_.clear();
    }
}

}  // namespace autocattery::ui
