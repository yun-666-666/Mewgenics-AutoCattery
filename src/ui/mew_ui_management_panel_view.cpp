#include "mew_ui_management_panel_view.hpp"

#include <array>
#include <string>

#include "mew_ui_movie_clip.hpp"
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#include "mew_ui_api.h"

namespace autocattery::ui {
namespace {

constexpr std::array<const char*, 7> kFixedNames{
    "panel_tab_settings", "panel_tab_protection", "panel_close",
    "panel_prev", "panel_next", "panel_apply", "panel_remove"
};
constexpr std::array<const char*, 3> kGroupNames{
    "战斗评分与推荐", "繁育评分与分类", "房间规则与安全"
};

std::string IndexedName(const char* prefix, std::size_t index) {
    return std::string(prefix) + std::to_string(index + 1);
}

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
    fixed_nodes_.fill(nullptr);
    protection_nodes_.fill(nullptr);
    group_nodes_.fill(nullptr);
    setting_nodes_.fill(nullptr);
}

Result<void> MewUiManagementPanelView::Show(
    const ManagementPanelContent& content) {
    if (!CanTouchScene()) {
        return {ErrorCode::SceneUnavailable, "House panel scene expired"};
    }
    MewUI_SetModalInputBlocked(true);
    visible_.store(true);
    protection_page_.store(content.protection_page);
    navigation_visible_.store(content.show_navigation);
    apply_visible_.store(content.show_apply);
    remove_visible_.store(content.show_remove);
    HoldMewUiMovieClipFrame(background_, 1);
    SetElement(fixed_nodes_[0], kFixedNames[0], "设置",
               content.protection_page ? 1 : 2);
    SetElement(fixed_nodes_[1], kFixedNames[1], "猫保护",
               content.protection_page ? 2 : 1);
    SetElement(fixed_nodes_[2], kFixedNames[2], "关闭", 1);
    SetElement(fixed_nodes_[3], kFixedNames[3],
               content.show_navigation ? "上一页" : "",
               content.show_navigation ? 1 : 0);
    SetElement(fixed_nodes_[4], kFixedNames[4],
               content.show_navigation ? "下一页" : "",
               content.show_navigation ? 1 : 0);
    SetElement(fixed_nodes_[5], kFixedNames[5],
               content.show_apply ? "应用" : "",
               content.show_apply ? 1 : 0);
    SetElement(fixed_nodes_[6], kFixedNames[6],
               content.show_remove ? "移除" : "",
               content.show_remove ? 1 : 0);

    for (std::size_t index = 0; index < protection_nodes_.size(); ++index) {
        const auto name = IndexedName("panel_protection_row_", index);
        const bool shown = content.protection_page &&
            index < content.rows.size() && !content.rows[index].empty();
        const int frame = content.selected_row == index ? 2 : 1;
        SetElement(protection_nodes_[index], name.c_str(),
                   shown ? content.rows[index].c_str() : "", shown ? frame : 0);
    }
    for (std::size_t index = 0; index < group_nodes_.size(); ++index) {
        const auto name = IndexedName("panel_group_", index);
        SetElement(group_nodes_[index], name.c_str(),
                   content.protection_page ? "" : kGroupNames[index],
                   content.protection_page ? 0 : 1);
    }
    for (std::size_t index = 0; index < setting_nodes_.size(); ++index) {
        const auto name = IndexedName("panel_setting_row_", index);
        const bool shown = !content.protection_page && index < content.rows.size();
        const int frame = content.selected_row == index ? 2 : 1;
        SetElement(setting_nodes_[index], name.c_str(),
                   shown ? content.rows[index].c_str() : "", shown ? frame : 0);
    }
    MewUI_SetTextInSceneText(scene_manager_, "panel_title", content.title.c_str());
    MewUI_SetTextInSceneText(scene_manager_, "panel_status", content.status.c_str());
    return {};
}

void MewUiManagementPanelView::Hide() noexcept {
    MewUI_SetModalInputBlocked(false);
    visible_.store(false);
    protection_page_.store(false);
    navigation_visible_.store(false);
    apply_visible_.store(false);
    remove_visible_.store(false);
    CancelNumericInput();
    pending_control_.store(-1);
    pending_row_.store(-1);
    pending_direction_.store(0);
    if (!CanTouchScene()) return;
    HoldMewUiMovieClipFrame(background_, 0);
    for (std::size_t index = 0; index < fixed_nodes_.size(); ++index)
        SetElement(fixed_nodes_[index], kFixedNames[index], "", 0);
    for (std::size_t index = 0; index < protection_nodes_.size(); ++index) {
        const auto name = IndexedName("panel_protection_row_", index);
        SetElement(protection_nodes_[index], name.c_str(), "", 0);
    }
    for (std::size_t index = 0; index < group_nodes_.size(); ++index) {
        const auto name = IndexedName("panel_group_", index);
        SetElement(group_nodes_[index], name.c_str(), "", 0);
    }
    for (std::size_t index = 0; index < setting_nodes_.size(); ++index) {
        const auto name = IndexedName("panel_setting_row_", index);
        SetElement(setting_nodes_[index], name.c_str(), "", 0);
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
    background_ = MewUI_FindNodeInSceneByName(scene_manager_, "panel_background");
    if (background_ == nullptr) return false;
    auto resolve = [this](auto& nodes, const char* prefix) {
        for (std::size_t index = 0; index < nodes.size(); ++index) {
            const auto name = IndexedName(prefix, index);
            nodes[index] = MewUI_FindNodeInSceneByName(scene_manager_, name.c_str());
            if (nodes[index] == nullptr || MewUI_FindNodeInSceneByName(
                    scene_manager_, (name + "_text").c_str()) == nullptr) return false;
        }
        return true;
    };
    for (std::size_t index = 0; index < fixed_nodes_.size(); ++index) {
        fixed_nodes_[index] = MewUI_FindNodeInSceneByName(
            scene_manager_, kFixedNames[index]);
        if (fixed_nodes_[index] == nullptr || MewUI_FindNodeInSceneByName(
                scene_manager_, (std::string(kFixedNames[index]) + "_text").c_str()) == nullptr)
            return false;
    }
    return resolve(protection_nodes_, "panel_protection_row_") &&
        resolve(group_nodes_, "panel_group_") &&
        resolve(setting_nodes_, "panel_setting_row_") &&
        MewUI_FindNodeInSceneByName(scene_manager_, "panel_title") &&
        MewUI_FindNodeInSceneByName(scene_manager_, "panel_status");
}

bool MewUiManagementPanelView::CanTouchScene() const noexcept {
    return scene_manager_ != nullptr &&
        MewUI_GetSceneByName("House") == scene_manager_ &&
        MewUI_IsSceneReadyForUITick(scene_manager_) != 0 &&
        MewUI_IsSceneDestroying(scene_manager_) == 0;
}

void MewUiManagementPanelView::SetElement(
    void* node, const char* name, const char* text, int frame) noexcept {
    MewUI_SetTextInSceneText(
        scene_manager_, (std::string(name) + "_text").c_str(), text);
    HoldMewUiMovieClipFrame(node, frame);
}

}  // namespace autocattery::ui
