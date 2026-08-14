#include "mew_ui_management_panel_view.hpp"

#include <array>
#include <chrono>
#include <string>

#include "mew_ui_movie_clip.hpp"
#include "mew_ui_safe_node_lookup.hpp"
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#include "mew_ui_api.h"

namespace autocattery::ui {
namespace {

constexpr std::array<const char*, 9> kFixedNames{
    "panel_tab_settings", "panel_tab_protection", "panel_tab_preview",
    "panel_tab_furniture", "panel_close", "panel_prev", "panel_next",
    "panel_apply", "panel_remove"
};

std::string IndexedName(const char* prefix, std::size_t index) {
    return std::string(prefix) + std::to_string(index + 1);
}

}  // namespace

MewUiManagementPanelView::~MewUiManagementPanelView() { Detach(); }

Result<void> MewUiManagementPanelView::Attach(
    const UiContextSnapshot& context) {
    const auto started = std::chrono::steady_clock::now();
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
    last_attach_elapsed_us_ = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started)
            .count());
    return {};
}

void MewUiManagementPanelView::Detach() noexcept {
    Hide();
    RemoveHook();
    scene_manager_ = nullptr;
    generation_ = 0;
    root_node_ = nullptr;
    background_ = nullptr;
    ResetElements();
    resolve_mode_ = "unresolved";
}

void MewUiManagementPanelView::AbandonScene() noexcept {
    MewUI_SetModalInputBlocked(false);
    visible_.store(false);
    CancelNumericInput();
    pending_control_.store(-1);
    pending_row_.store(-1);
    pending_direction_.store(0);
    RemoveHook();
    scene_manager_ = nullptr;
    generation_ = 0;
    root_node_ = nullptr;
    background_ = nullptr;
    ResetElements();
    resolve_mode_ = "unresolved";
}

Result<void> MewUiManagementPanelView::Show(
    const ManagementPanelContent& content) {
    const auto started = std::chrono::steady_clock::now();
    if (!CanTouchScene()) {
        return {ErrorCode::SceneUnavailable, "House panel scene expired"};
    }
    changed_text_count_ = 0;
    changed_frame_count_ = 0;
    MewUI_SetModalInputBlocked(true);
    visible_.store(true);
    page_.store(static_cast<int>(content.page));
    navigation_visible_.store(content.show_navigation);
    apply_visible_.store(content.show_apply);
    remove_visible_.store(content.show_remove);
    HoldMewUiMovieClipFrame(background_, 1);
    const bool english = !content.group_titles.empty() &&
        (content.group_titles.front() == "Combat Scoring" ||
         content.group_titles.front() == "Breeding / Kitten & Recovery");
    SetElement(fixed_nodes_[0], english ? "Settings" : "设置",
               content.page == ManagementPanelPage::Settings ? 2 : 1);
    SetElement(fixed_nodes_[1], english ? "Cat Protection" : "猫保护",
               content.page == ManagementPanelPage::Protection ? 2 : 1);
    SetElement(fixed_nodes_[2], english ? "Full Preview" : "完整预览",
               content.page == ManagementPanelPage::Preview ? 2 : 1);
    SetElement(fixed_nodes_[3], english ? "Auto Placement" : "自动放置",
               content.page == ManagementPanelPage::Furniture ? 2 : 1);
    SetElement(fixed_nodes_[4], english ? "Close" : "关闭", 1);
    SetElement(fixed_nodes_[5],
               content.show_navigation ? (english ? "Previous" : "上一页") : "",
               content.show_navigation ? 1 : 0);
    SetElement(fixed_nodes_[6],
               content.show_navigation ? (english ? "Next" : "下一页") : "",
               content.show_navigation ? 1 : 0);
    SetElement(fixed_nodes_[7],
               content.show_apply ? (english ? "Apply" : "应用") : "",
               content.show_apply ? 1 : 0);
    SetElement(fixed_nodes_[8],
               content.show_remove ? (english ? "Remove" : "移除") : "",
               content.show_remove ? 1 : 0);

    for (std::size_t index = 0; index < list_nodes_.size(); ++index) {
        const bool shown = content.page != ManagementPanelPage::Settings &&
            content.page != ManagementPanelPage::Furniture &&
            index < content.rows.size() && !content.rows[index].empty();
        const int frame = content.selected_row == index ? 2 : 1;
        SetElement(list_nodes_[index],
                   shown ? content.rows[index].c_str() : "", shown ? frame : 0);
    }
    for (std::size_t index = 0; index < group_nodes_.size(); ++index) {
        const bool shown =
            (content.page == ManagementPanelPage::Settings ||
             content.page == ManagementPanelPage::Furniture) &&
            index < content.group_titles.size();
        SetElement(group_nodes_[index],
                   shown ? content.group_titles[index].c_str() : "",
                   shown ? 1 : 0);
    }
    for (std::size_t index = 0; index < setting_nodes_.size(); ++index) {
        const bool shown =
            (content.page == ManagementPanelPage::Settings ||
             content.page == ManagementPanelPage::Furniture) &&
            index < content.rows.size() && !content.rows[index].empty();
        setting_row_visible_[index].store(shown);
        const int frame = content.selected_row == index ? 2 : 1;
        SetElement(setting_nodes_[index],
                   shown ? content.rows[index].c_str() : "", shown ? frame : 0);
    }
    SetElement(title_, content.title.c_str(), 0);
    SetElement(status_, content.status.c_str(), 0);
    last_render_elapsed_us_ = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started)
            .count());
    last_changed_text_count_ = changed_text_count_;
    last_changed_frame_count_ = changed_frame_count_;
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
    for (std::size_t index = 0; index < fixed_nodes_.size(); ++index)
        SetElement(fixed_nodes_[index], "", 0);
    for (auto& node : list_nodes_) SetElement(node, "", 0);
    for (std::size_t index = 0; index < group_nodes_.size(); ++index) {
        SetElement(group_nodes_[index], "", 0);
    }
    for (std::size_t index = 0; index < setting_nodes_.size(); ++index) {
        SetElement(setting_nodes_[index], "", 0);
        setting_row_visible_[index].store(false);
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

const char* MewUiManagementPanelView::ResolveMode() const noexcept {
    return resolve_mode_;
}

std::uint64_t MewUiManagementPanelView::LastAttachElapsedUs() const noexcept {
    return last_attach_elapsed_us_;
}

std::uint64_t MewUiManagementPanelView::LastRenderElapsedUs() const noexcept {
    return last_render_elapsed_us_;
}

std::uint32_t MewUiManagementPanelView::LastChangedTextCount() const noexcept {
    return last_changed_text_count_;
}

std::uint32_t MewUiManagementPanelView::LastChangedFrameCount() const noexcept {
    return last_changed_frame_count_;
}

bool MewUiManagementPanelView::ResolveNodes() noexcept {
    const auto match = FindSceneUiNode(scene_manager_, "panel_background");
    root_node_ = match.root;
    background_ = match.node;
    if (root_node_ != nullptr && background_ != nullptr &&
        ResolveNodesInRoot(root_node_)) {
        resolve_mode_ = "safe-root";
        return true;
    }
    return false;
}

bool MewUiManagementPanelView::ResolveNodesInRoot(void* root_node) noexcept {
    if (root_node == nullptr || background_ == nullptr) return false;
    auto find = [root_node](const std::string& name) {
        return MewUI_FindChildByName(root_node, name.c_str());
    };
    auto resolve = [this](auto& nodes, const char* prefix) {
        for (std::size_t index = 0; index < nodes.size(); ++index) {
            const auto name = IndexedName(prefix, index);
            nodes[index].clip = MewUI_FindChildByName(
                root_node_, name.c_str());
            nodes[index].text = MewUI_FindChildByName(
                root_node_, (name + "_text").c_str());
            if (nodes[index].clip == nullptr || nodes[index].text == nullptr)
                return false;
        }
        return true;
    };
    for (std::size_t index = 0; index < fixed_nodes_.size(); ++index) {
        fixed_nodes_[index].clip = find(kFixedNames[index]);
        fixed_nodes_[index].text = find(
            std::string(kFixedNames[index]) + "_text");
        if (fixed_nodes_[index].clip == nullptr ||
            fixed_nodes_[index].text == nullptr)
            return false;
    }
    title_.text = find("panel_title");
    status_.text = find("panel_status");
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
        ++changed_text_count_;
    }
    if (element.clip != nullptr && element.rendered_frame != frame) {
        HoldMewUiMovieClipFrame(element.clip, frame);
        element.rendered_frame = frame;
        ++changed_frame_count_;
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
