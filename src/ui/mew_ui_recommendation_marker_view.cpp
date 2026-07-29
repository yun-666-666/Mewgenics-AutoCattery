#include "mew_ui_recommendation_marker_view.hpp"

#include <utility>

namespace autocattery::ui {
namespace {

constexpr auto kButtonNode = "recommend_button";
constexpr auto kButtonRole =
    "AutoCattery.Recommendation.MarkCombatCatsButton";
constexpr auto kReadyText = "HOUSE.RECOMMEND_COMBAT_CATS";

}  // namespace

Result<void> MewUiRecommendationMarkerView::Attach(
    const UiContextSnapshot& context,
    ClickHandler click_handler) {
    auto* scene = MewUI_GetSceneByName(context.scene_name.c_str());
    if (scene == nullptr ||
        MewUI_IsSceneReadyForUITick(scene) == 0 ||
        MewUI_IsSceneDestroying(scene) != 0) {
        return {
            ErrorCode::SceneUnavailable,
            "house scene is not ready for recommendation UI attachment"
        };
    }

    if (scene_manager_ == scene &&
        button_ != nullptr &&
        MewUI_IsComponentInScene(scene, button_) != 0) {
        click_handler_ = std::move(click_handler);
        active_ = true;
        MewUI_SetButtonEnabled(button_, 1);
        MewUI_SetButtonInteractable(button_, 1);
        return {};
    }

    scene_manager_ = scene;
    button_ = nullptr;
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
            "the dedicated recommendation button asset is unavailable"
        };
    }

    active_ = true;
    MewUI_SetButtonEnabled(button_, 1);
    MewUI_SetButtonInteractable(button_, 1);
    return {};
}

void MewUiRecommendationMarkerView::Detach() noexcept {
    if (scene_manager_ != nullptr &&
        button_ != nullptr &&
        MewUI_IsSceneDestroying(scene_manager_) == 0 &&
        MewUI_IsComponentInScene(scene_manager_, button_) != 0) {
        MewUI_SetButtonInteractable(button_, 0);
        MewUI_SetButtonEnabled(button_, 0);
    } else {
        scene_manager_ = nullptr;
        button_ = nullptr;
    }
    active_ = false;
    click_handler_ = {};
}

void MewUiRecommendationMarkerView::SetStatus(
    RecommendationUiStatus status) {
    if (button_ == nullptr) {
        return;
    }
    if (status == RecommendationUiStatus::ProbeRequired) {
        MewUI_SetButtonLabelText(button_, "Probe Required");
        return;
    }
    MewUI_SetButtonLabelFromLocalizationKey(button_, kReadyText);
}

bool MewUiRecommendationMarkerView::IsAttached() const noexcept {
    return active_ && scene_manager_ != nullptr && button_ != nullptr;
}

void __cdecl MewUiRecommendationMarkerView::ButtonCallback(
    void* button,
    MewButtonEvent event_type,
    MewButtonState old_state,
    MewButtonState new_state,
    void* user_data) {
    (void)button;
    (void)old_state;
    (void)new_state;
    auto* self =
        static_cast<MewUiRecommendationMarkerView*>(user_data);
    if (self != nullptr &&
        event_type == MEW_BUTTON_EVENT_CLICK &&
        self->click_handler_) {
        self->click_handler_();
    }
}

}  // namespace autocattery::ui
