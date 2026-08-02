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

constexpr std::array<const char*, 8> kFixedNames{
    "panel_tab_settings", "panel_tab_protection", "panel_tab_preview",
    "panel_close", "panel_prev", "panel_next", "panel_apply", "panel_remove"
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
    ResetElements();
}

Result<void> MewUiManagementPanelView::Show(
    const ManagementPanelContent& content) {
    if (!CanTouchScene()) {
        return {ErrorCode::SceneUnavailable, "House panel scene expired"};
    }
    MewUI_SetModalInputBlocked(true);
    visible_.store(true);
    page_.store(static_cast<int>(content.page));
    navigation_visible_.store(content.show_navigation);
    apply_visible_.store(content.show_apply);
    remove_visible_.store(content.show_remove);
    // Keep the whole composition hidden while text and control frames are
    // being changed. The background is published only after all rows below
    // are ready.
    HoldMewUiMovieClipFrame(background_, 0);
    const bool english = !content.group_titles.empty() &&
        content.group_titles.front() == "Combat Scoring";
    SetElement(fixed_nodes_[0], english ? "Settings" : "设置",
               content.page == ManagementPanelPage::Settings ? 2 : 1);
    SetElement(fixed_nodes_[1], english ? "Cat Protection" : "猫保护",
               content.page == ManagementPanelPage::Protection ? 2 : 1);
    SetElement(fixed_nodes_[2], english ? "Full Preview" : "完整预览",
               content.page == ManagementPanelPage::Preview ? 2 : 1);
    SetElement(fixed_nodes_[3], english ? "Close" : "关闭", 1);
    SetElement(fixed_nodes_[4],
               content.show_navigation ? (english ? "Previous" : "上一页") : "",
               content.show_navigation ? 1 : 0);
    SetElement(fixed_nodes_[5],
               content.show_navigation ? (english ? "Next" : "下一页") : "",
               content.show_navigation ? 1 : 0);
    SetElement(fixed_nodes_[6],
               content.show_apply ? (english ? "Apply" : "应用") : "",
               content.show_apply ? 1 : 0);
    SetElement(fixed_nodes_[7],
               content.show_remove ? (english ? "Remove" : "移除") : "",
               content.show_remove ? 1 : 0);

    for (std::size_t index = 0; index < list_nodes_.size(); ++index) {
        const bool shown = content.page != ManagementPanelPage::Settings &&
            index < content.rows.size() && !content.rows[index].empty();
        const int frame = content.selected_row == index ? 2 : 1;
        SetElement(list_nodes_[index],
                   shown ? content.rows[index].c_str() : "", shown ? frame : 0);
    }
    for (std::size_t index = 0; index < group_nodes_.size(); ++index) {
        const bool shown = content.page == ManagementPanelPage::Settings &&
            index < content.group_titles.size();
        SetElement(group_nodes_[index],
                   shown ? content.group_titles[index].c_str() : "",
                   shown ? 1 : 0);
    }
    for (std::size_t index = 0; index < setting_nodes_.size(); ++index) {
        const bool shown = content.page == ManagementPanelPage::Settings &&
            index < content.rows.size();
        const int frame = content.selected_row == index ? 2 : 1;
        SetElement(setting_nodes_[index],
                   shown ? content.rows[index].c_str() : "", shown ? frame : 0);
    }
    SetElement(title_, content.title.c_str(), 0);
    SetElement(status_, content.status.c_str(), 0);
    // Publish the background only after all cached rows have been populated.
    // This prevents a visible frame of empty SWF rectangles while the text
    // elements are being updated on the game UI thread.
    HoldMewUiMovieClipFrame(background_, 1);
    return {};
}

void MewUiManagementPanelView::Hide() noexcept {
    MewUI_SetModalInputBlocked(false);
    visible_.store(false);
    page_.store(static_cast<int>(ManagementPanelPage::Settings));
    navigation_visible_.store(false);
    apply_visible_.store(false);
    remove_visible_.store(false);
    CancelNumericInput();
    pending_control_.store(-1);
    pending_row_.store(-1);
    pending_direction_.store(0);
    if (!CanTouchScene()) return;
    HoldMewUiMovieClipFrame(background_, 0);
    // Hide every independent control clip and clear the independent text
    // nodes. The background alone does not cover these nodes, and text
    // elements are not children of the movie-clip frame.
    for (auto& node : fixed_nodes_) {
        SetElement(node, "", 0);
    }
    for (auto& node : list_nodes_) {
        SetElement(node, "", 0);
    }
    for (auto& node : group_nodes_) {
        SetElement(node, "", 0);
    }
    for (auto& node : setting_nodes_) {
        SetElement(node, "", 0);
    }
    SetElement(title_, "", 0);
    SetElement(status_, "", 0);
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
            nodes[index].clip = MewUI_FindNodeInSceneByName(
                scene_manager_, name.c_str());
            nodes[index].text = MewUI_FindNodeInSceneByName(
                scene_manager_, (name + "_text").c_str());
            if (nodes[index].clip == nullptr || nodes[index].text == nullptr)
                return false;
        }
        return true;
    };
    for (std::size_t index = 0; index < fixed_nodes_.size(); ++index) {
        fixed_nodes_[index].clip = MewUI_FindNodeInSceneByName(
            scene_manager_, kFixedNames[index]);
        fixed_nodes_[index].text = MewUI_FindNodeInSceneByName(
            scene_manager_,
            (std::string(kFixedNames[index]) + "_text").c_str());
        if (fixed_nodes_[index].clip == nullptr ||
            fixed_nodes_[index].text == nullptr)
            return false;
    }
    title_.text = MewUI_FindNodeInSceneByName(scene_manager_, "panel_title");
    status_.text = MewUI_FindNodeInSceneByName(scene_manager_, "panel_status");
    return resolve(list_nodes_, "panel_protection_row_") &&
        resolve(group_nodes_, "panel_group_") &&
        resolve(setting_nodes_, "panel_setting_row_") &&
        title_.text != nullptr && status_.text != nullptr;
}

bool MewUiManagementPanelView::CanTouchScene() const noexcept {
    return scene_manager_ != nullptr &&
        MewUI_GetSceneByName("House") == scene_manager_ &&
        MewUI_IsSceneReadyForUITick(scene_manager_) != 0 &&
        MewUI_IsSceneDestroying(scene_manager_) == 0;
}

void MewUiManagementPanelView::SetElement(
    Element& element, const char* text, int frame) noexcept {
    if (element.text != nullptr && element.rendered_text != text) {
        MewUI_SetTextElementText(element.text, text);
        element.rendered_text = text;
    }
    if (element.clip != nullptr && element.rendered_frame != frame) {
        HoldMewUiMovieClipFrame(element.clip, frame);
        element.rendered_frame = frame;
    }
}

void MewUiManagementPanelView::ResetElements() noexcept {
    for (auto& node : fixed_nodes_) node = {};
    for (auto& node : list_nodes_) node = {};
    for (auto& node : group_nodes_) node = {};
    for (auto& node : setting_nodes_) node = {};
    title_ = {};
    status_ = {};
}

}  // namespace autocattery::ui
