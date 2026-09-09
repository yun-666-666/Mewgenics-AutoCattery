#include "mew_ui_house_button_view.hpp"

#include <cstring>
#include <utility>

#include "mew_ui_movie_clip.hpp"
#include "mew_ui_safe_node_lookup.hpp"

namespace autocattery::ui {
namespace {

constexpr auto kButtonNode = "test_button";
constexpr auto kButtonRole = "AutoCattery.House.AutoOrganizeButton";

}  // namespace

Result<void> MewUiHouseButtonView::Attach(
    const UiContextSnapshot& context,
    ClickHandler click_handler) {
    auto* scene = MewUI_GetSceneByName(context.scene_name.c_str());
    if (scene == nullptr ||
        MewUI_IsSceneReadyForUITick(scene) == 0 ||
        MewUI_IsSceneDestroying(scene) != 0) {
        return {
            ErrorCode::SceneUnavailable,
            "house scene is not ready for UI attachment"
        };
    }

    scene_manager_ = scene;
    button_ = nullptr;
    attached_generation_ = context.scene_generation;
    active_ = false;
    click_handler_ = std::move(click_handler);
    const auto match = FindSceneUiNode(scene_manager_, kButtonNode);
    auto* button_node = match.node;
    if (match.root == nullptr || button_node == nullptr) {
        ResetSceneState();
        return {
            ErrorCode::UiNodeNotFound,
            "the AutoCattery house button asset is unavailable"
        };
    }
    // Native Button setup resolves the frame-owned state nodes and hit area.
    // Frame 0 is intentionally empty, so materialize the stopped visible
    // frame before setup and let setup replace the source label in this tick.
    if (!HoldMewUiMovieClipFrame(button_node, 1)) {
        ResetSceneState();
        return {
            ErrorCode::UiNodeNotFound,
            "the AutoCattery house button could not be prepared"
        };
    }
    MewButtonCreateInfo create_info{};
    create_info.scene_manager = scene_manager_;
    create_info.root_node = match.root;
    create_info.button_node = button_node;
    create_info.node_name = kButtonNode;
    create_info.role_name = kButtonRole;
    create_info.label_text = english_
        ? "Auto-Organize Cattery" : "自动整理猫舍";
    create_info.enabled = 1;
    create_info.activate_enabled = 1;
    create_info.strict_mouse = 1;
    create_info.interact_override = MEW_BUTTON_INTERACT_FORCE_ENABLED;
    create_info.callback = &ButtonCallback;
    create_info.user_data = this;
    int created{};
    button_ = MewUI_SetupButtonFromNode(
        &create_info,
        &button_,
        &created);
    if (button_ == nullptr) {
        HoldMewUiMovieClipFrame(button_node, 0);
        ResetSceneState();
        return {
            ErrorCode::UiNodeNotFound,
            "the AutoCattery house button asset is unavailable"
        };
    }
    MewUI_RegisterExistingButton(
        button_,
        kButtonRole,
        &ButtonCallback,
        this);

    active_ = true;
    MewUI_SetButtonEnabled(button_, 1);
    MewUI_SetButtonInteractable(button_, 1);
    return {};
}

void MewUiHouseButtonView::Detach() noexcept {
    const bool can_touch_scene = CanTouchScene();
    if (can_touch_scene && button_ != nullptr &&
        MewUI_IsComponentInScene(scene_manager_, button_) != 0) {
        MewUI_SetButtonInteractable(button_, 0);
        MewUI_SetButtonEnabled(button_, 0);
    }
    ResetSceneState();
}

void MewUiHouseButtonView::AbandonScene() noexcept {
    if (button_ != nullptr) {
        if (auto* record = MewUI_GetButtonRecord(button_); record != nullptr) {
            std::memset(record, 0, sizeof(*record));
        }
    }
    ResetSceneState();
}

void MewUiHouseButtonView::ResetSceneState() noexcept {
    active_ = false;
    scene_manager_ = nullptr;
    button_ = nullptr;
    attached_generation_ = 0;
    click_handler_ = {};
}

void MewUiHouseButtonView::SetState(
    OrganizeButtonState state,
    std::string_view detail) {
    current_state_ = state;
    if (button_ == nullptr || !CanTouchScene()) {
        return;
    }

    const char* label = english_ ? "Auto-Organize Cattery" : "自动整理猫舍";
    bool enabled = true;
    switch (state) {
    case OrganizeButtonState::Hidden:
        enabled = false;
        break;
    case OrganizeButtonState::DisabledUnsupportedBuild:
        label = english_ ? "Preview Only" : "仅预览";
        enabled = false;
        break;
    case OrganizeButtonState::DisabledBusy:
    case OrganizeButtonState::Running:
        label = english_ ? "Analyzing…" : "正在分析…";
        enabled = false;
        break;
    case OrganizeButtonState::Ready:
        label = detail.starts_with("Organize preview:")
            ? (english_ ? "Confirm + Clear Poop" : "确认整理并清便")
            : (english_ ? "Auto-Organize Cattery" : "自动整理猫舍");
        break;
    case OrganizeButtonState::Completed:
        label = english_ ? "Preview Complete" : "预览已完成";
        enabled = false;
        break;
    case OrganizeButtonState::Failed:
        label = detail.find("poop cleanup failed") != std::string_view::npos
            ? (english_ ? "Cleanup Failed" : "清便失败，请查日志")
            : (english_ ? "Preview Unavailable" : "预览不可用");
        enabled = false;
        break;
    }
    MewUI_SetButtonLabelText(button_, label);
    MewUI_SetButtonInteractable(button_, enabled ? 1 : 0);
    MewUI_SetButtonEnabled(button_, enabled ? 1 : 0);
}

void MewUiHouseButtonView::ShowPlaceholder() {
    // The compact top-right layout uses the button label as its complete
    // feedback surface. Avoid scene-wide text-node probes here: the pinned
    // MewUI build handles a missing text node through repeated SEH probes,
    // which caused a visible pause on every Stage 03 click.
}

void MewUiHouseButtonView::SetEnglish(bool english) {
    english_ = english;
    if (button_ != nullptr && CanTouchScene()) SetState(current_state_, {});
}

bool MewUiHouseButtonView::IsAttached() const noexcept {
    return active_ && scene_manager_ != nullptr && button_ != nullptr;
}

bool MewUiHouseButtonView::CanTouchScene() const noexcept {
    if (scene_manager_ == nullptr ||
        MewUI_GetSceneByName("House") != scene_manager_) {
        return false;
    }
    return MewUI_IsSceneReadyForUITick(scene_manager_) != 0 &&
           MewUI_IsSceneDestroying(scene_manager_) == 0;
}

void __cdecl MewUiHouseButtonView::ButtonCallback(
    void* button,
    MewButtonEvent event_type,
    MewButtonState old_state,
    MewButtonState new_state,
    void* user_data) {
    (void)button;
    (void)old_state;
    (void)new_state;
    auto* self = static_cast<MewUiHouseButtonView*>(user_data);
    if (self != nullptr &&
        event_type == MEW_BUTTON_EVENT_CLICK &&
        self->click_handler_) {
        self->click_handler_();
    }
}

}  // namespace autocattery::ui
