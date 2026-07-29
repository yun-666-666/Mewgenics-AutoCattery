#include "mew_ui_house_button_view.hpp"

#include <utility>

namespace autocattery::ui {
namespace {

constexpr auto kButtonNode = "test_button";
constexpr auto kButtonRole = "AutoCattery.House.AutoOrganizeButton";

constexpr auto kReadyText = "HOUSE.AUTO_ORGANIZE";
constexpr auto kRunningText = "HOUSE.RUNNING";
constexpr auto kCompletedText = "HOUSE.COMPLETED";
constexpr auto kUnsupportedText = "HOUSE.UNSUPPORTED";
constexpr auto kFailedText = "HOUSE.FAILED";

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

    if (scene_manager_ == scene &&
        button_ != nullptr &&
        MewUI_IsComponentInScene(scene, button_) != 0) {
        attached_generation_ = context.scene_generation;
        click_handler_ = std::move(click_handler);
        active_ = true;
        MewUI_SetButtonEnabled(button_, 1);
        MewUI_SetButtonInteractable(button_, 1);
        return {};
    }

    scene_manager_ = scene;
    button_ = nullptr;
    attached_generation_ = context.scene_generation;
    active_ = false;
    click_handler_ = std::move(click_handler);
    auto* button_node =
        MewUI_FindNodeInSceneByName(scene_manager_, kButtonNode);
    MewButtonCreateInfo create_info{};
    create_info.scene_manager = scene_manager_;
    create_info.button_node = button_node;
    create_info.node_name = kButtonNode;
    create_info.role_name = kButtonRole;
    create_info.label_key = kReadyText;
    create_info.enabled = 1;
    create_info.activate_enabled = 1;
    create_info.strict_mouse = 1;
    create_info.interact_override = MEW_BUTTON_INTERACT_FORCE_ENABLED;
    create_info.callback = &ButtonCallback;
    create_info.user_data = this;
    button_ = MewUI_CreateButtonFromNode(&create_info);
    if (button_ == nullptr) {
        scene_manager_ = nullptr;
        click_handler_ = {};
        return {
            ErrorCode::UiNodeNotFound,
            "the AutoCattery house button asset is unavailable"
        };
    }

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
    if (!can_touch_scene) {
        scene_manager_ = nullptr;
        button_ = nullptr;
        attached_generation_ = 0;
    }
    active_ = false;
    click_handler_ = {};
}

void MewUiHouseButtonView::SetState(
    OrganizeButtonState state,
    std::string_view detail) {
    (void)detail;
    if (button_ == nullptr || !CanTouchScene()) {
        return;
    }

    const char* label = kReadyText;
    bool enabled = true;
    switch (state) {
    case OrganizeButtonState::Hidden:
        enabled = false;
        break;
    case OrganizeButtonState::DisabledUnsupportedBuild:
        label = kUnsupportedText;
        enabled = false;
        break;
    case OrganizeButtonState::DisabledBusy:
    case OrganizeButtonState::Running:
        label = kRunningText;
        enabled = false;
        break;
    case OrganizeButtonState::Ready:
        label = kReadyText;
        break;
    case OrganizeButtonState::Completed:
        label = kCompletedText;
        enabled = false;
        break;
    case OrganizeButtonState::Failed:
        label = kFailedText;
        enabled = false;
        break;
    }
    MewUI_SetButtonLabelFromLocalizationKey(button_, label);
    MewUI_SetButtonInteractable(button_, enabled ? 1 : 0);
    MewUI_SetButtonEnabled(button_, enabled ? 1 : 0);
}

void MewUiHouseButtonView::ShowPlaceholder() {
    // The compact top-right layout uses the button label as its complete
    // feedback surface. Avoid scene-wide text-node probes here: the pinned
    // MewUI build handles a missing text node through repeated SEH probes,
    // which caused a visible pause on every Stage 03 click.
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
