#include "mew_ui_management_panel_view.hpp"

#include <algorithm>

#include "mew_ui_movie_clip.hpp"
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#include "mew_ui_api.h"

namespace autocattery::ui {
namespace {

constexpr std::array<const char*, 15> kNodes{
    "panel_tab_settings", "panel_tab_protection", "panel_close",
    "panel_prev", "panel_next", "panel_apply", "panel_remove",
    "panel_row_1", "panel_row_2", "panel_row_3", "panel_row_4",
    "panel_row_5", "panel_row_6", "panel_row_7", "panel_row_8"
};
constexpr std::array<const char*, 15> kTexts{
    "panel_tab_settings_text", "panel_tab_protection_text",
    "panel_close_text", "panel_prev_text", "panel_next_text",
    "panel_apply_text", "panel_remove_text", "panel_row_1_text",
    "panel_row_2_text", "panel_row_3_text", "panel_row_4_text",
    "panel_row_5_text", "panel_row_6_text", "panel_row_7_text",
    "panel_row_8_text"
};
}  // namespace

MewUiManagementPanelView::~MewUiManagementPanelView() { Detach(); }

Result<void> MewUiManagementPanelView::Attach(
    const UiContextSnapshot& context) {
    auto* scene = MewUI_GetSceneByName(context.scene_name.c_str());
    if (scene == nullptr || MewUI_IsSceneReadyForUITick(scene) == 0 ||
        MewUI_IsSceneDestroying(scene) != 0) {
        return {ErrorCode::SceneUnavailable,
                "House is not ready for the F10 panel"};
    }
    scene_manager_ = scene;
    generation_ = context.scene_generation;
    if (!ResolveNodes()) {
        Detach();
        return {ErrorCode::UiNodeNotFound,
                "F10 panel nodes are missing from the House SWF"};
    }
    if (!InstallHook()) {
        Detach();
        return {ErrorCode::InternalError,
                "F10 panel input hook could not be installed"};
    }
    Hide();
    return {};
}

void MewUiManagementPanelView::Detach() noexcept {
    Hide();
    RemoveHook();
    scene_manager_ = nullptr;
    generation_ = 0;
    background_ = nullptr;
    elements_.fill(nullptr);
}

Result<void> MewUiManagementPanelView::Show(
    const ManagementPanelContent& content) {
    if (!CanTouchScene()) {
        return {ErrorCode::SceneUnavailable, "House panel scene expired"};
    }
    visible_.store(true);
    HoldMewUiMovieClipFrame(background_, 1);
    SetElement(0, "设置", content.protection_page ? 1 : 2);
    SetElement(1, "猫保护", content.protection_page ? 2 : 1);
    SetElement(2, "关闭", 1);
    SetElement(3, "上一页", 1);
    SetElement(4, "下一页", 1);
    SetElement(5, content.show_apply ? "应用" : "", 1);
    SetElement(6, content.show_remove ? "移除" : "", 1);
    for (std::size_t row = 0; row < 8; ++row) {
        const bool shown = row < content.rows.size();
        const int frame = content.selected_row == row ? 2 : 1;
        SetElement(7 + row, shown ? content.rows[row].c_str() : "",
                   shown ? frame : 0);
    }
    MewUI_SetTextInSceneText(
        scene_manager_, "panel_title", content.title.c_str());
    MewUI_SetTextInSceneText(
        scene_manager_, "panel_status", content.status.c_str());
    return {};
}

void MewUiManagementPanelView::Hide() noexcept {
    visible_.store(false);
    pending_control_.store(-1);
    pending_row_.store(-1);
    pending_direction_.store(0);
    if (!CanTouchScene()) return;
    HoldMewUiMovieClipFrame(background_, 0);
    for (std::size_t index = 0; index < elements_.size(); ++index) {
        SetElement(index, "", 0);
    }
    MewUI_SetTextInSceneText(scene_manager_, "panel_title", "");
    MewUI_SetTextInSceneText(scene_manager_, "panel_status", "");
}

bool MewUiManagementPanelView::IsAttached() const noexcept {
    return scene_manager_ != nullptr && background_ != nullptr;
}

bool MewUiManagementPanelView::IsVisible() const noexcept {
    return visible_.load();
}

bool MewUiManagementPanelView::ResolveNodes() noexcept {
    background_ = MewUI_FindNodeInSceneByName(
        scene_manager_, "panel_background");
    if (background_ == nullptr) return false;
    for (std::size_t index = 0; index < kNodes.size(); ++index) {
        elements_[index] = MewUI_FindNodeInSceneByName(
            scene_manager_, kNodes[index]);
        if (elements_[index] == nullptr ||
            MewUI_FindNodeInSceneByName(scene_manager_, kTexts[index]) == nullptr) {
            return false;
        }
    }
    return MewUI_FindNodeInSceneByName(scene_manager_, "panel_title") &&
           MewUI_FindNodeInSceneByName(scene_manager_, "panel_status");
}

bool MewUiManagementPanelView::CanTouchScene() const noexcept {
    return scene_manager_ != nullptr &&
        MewUI_GetSceneByName("House") == scene_manager_ &&
        MewUI_IsSceneReadyForUITick(scene_manager_) != 0 &&
        MewUI_IsSceneDestroying(scene_manager_) == 0;
}

void MewUiManagementPanelView::SetElement(
    std::size_t index, const char* text, int frame) noexcept {
    MewUI_SetTextInSceneText(scene_manager_, kTexts[index], text);
    HoldMewUiMovieClipFrame(elements_[index], frame);
}

}  // namespace autocattery::ui
