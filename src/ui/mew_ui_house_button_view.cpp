#include "mew_ui_house_button_view.hpp"

#include <utility>

namespace autocattery::ui {
namespace {

constexpr auto kButtonNode = "test_button";
constexpr auto kButtonRole = "AutoCattery.House.AutoOrganizeButton";
constexpr auto kTitleNode = "test_text";
constexpr auto kBodyNode = "test_text_2";

constexpr auto kEmptyText = "HOUSE.EMPTY";
constexpr auto kReadyText = "HOUSE.AUTO_ORGANIZE";
constexpr auto kRunningText = "HOUSE.RUNNING";
constexpr auto kCompletedText = "HOUSE.COMPLETED";
constexpr auto kUnsupportedText = "HOUSE.UNSUPPORTED";
constexpr auto kFailedText = "HOUSE.FAILED";
constexpr auto kPlaceholderTitle = "HOUSE.PLACEHOLDER_TITLE";
constexpr auto kPlaceholderBody = "HOUSE.PLACEHOLDER_BODY";

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
    click_handler_ = std::move(click_handler);
    int created = 0;
    button_ = MewUI_SetupButtonFromLocalizationKey(
        context.scene_name.c_str(),
        kButtonNode,
        kButtonRole,
        kReadyText,
        &ButtonCallback,
        this,
        &button_,
        &created);
    if (button_ == nullptr) {
        scene_manager_ = nullptr;
        click_handler_ = {};
        return {
            ErrorCode::UiNodeNotFound,
            "the AutoCattery house button asset is unavailable"
        };
    }

    (void)created;
    MewUI_SetButtonEnabled(button_, 1);
    MewUI_SetButtonInteractable(button_, 1);
    MewUI_SetTextInSceneFromLocalizationKey(
        scene_manager_,
        kTitleNode,
        kEmptyText);
    MewUI_SetTextInSceneFromLocalizationKey(
        scene_manager_,
        kBodyNode,
        kEmptyText);
    return {};
}

void MewUiHouseButtonView::Detach() noexcept {
    if (scene_manager_ != nullptr &&
        button_ != nullptr &&
        MewUI_IsSceneDestroying(scene_manager_) == 0 &&
        MewUI_IsComponentInScene(scene_manager_, button_) != 0) {
        MewUI_SetButtonInteractable(button_, 0);
        MewUI_SetButtonEnabled(button_, 0);
    }
    scene_manager_ = nullptr;
    button_ = nullptr;
    click_handler_ = {};
}

void MewUiHouseButtonView::SetState(
    OrganizeButtonState state,
    std::string_view detail) {
    (void)detail;
    if (button_ == nullptr) {
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
        break;
    case OrganizeButtonState::Failed:
        label = kFailedText;
        break;
    }
    MewUI_SetButtonLabelFromLocalizationKey(button_, label);
    MewUI_SetButtonInteractable(button_, enabled ? 1 : 0);
    MewUI_SetButtonEnabled(button_, enabled ? 1 : 0);
}

void MewUiHouseButtonView::ShowPlaceholder() {
    if (scene_manager_ == nullptr) {
        return;
    }
    MewUI_SetTextInSceneFromLocalizationKey(
        scene_manager_,
        kTitleNode,
        kPlaceholderTitle);
    MewUI_SetTextInSceneFromLocalizationKey(
        scene_manager_,
        kBodyNode,
        kPlaceholderBody);
}

bool MewUiHouseButtonView::IsAttached() const noexcept {
    return scene_manager_ != nullptr && button_ != nullptr;
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
