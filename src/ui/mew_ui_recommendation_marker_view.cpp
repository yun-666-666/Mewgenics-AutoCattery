#include "mew_ui_recommendation_marker_view.hpp"

#include <utility>

namespace autocattery::ui {
namespace {

constexpr auto kButtonNode = "recommendation_button";
constexpr auto kButtonRole =
    "AutoCattery.House.MarkRecommendedCombatCatsButton";
constexpr auto kMarkerTextNode = "recommendation_text";
constexpr auto kExplanationTextNode = "recommendation_text_2";

constexpr auto kEmptyText = "EMBARK.EMPTY";
constexpr auto kReadyText = "EMBARK.MARK_RECOMMENDED";
constexpr auto kMarkingText = "EMBARK.MARKING";
constexpr auto kMarkedText = "EMBARK.CLEAR_MARKERS";
constexpr auto kDemoMarkerText = "EMBARK.DEMO_MARKER";
constexpr auto kDemoExplanationText = "EMBARK.DEMO_EXPLANATION";
constexpr std::int32_t kVisibleFrame = 0;
constexpr std::int32_t kHiddenFrame = 80;

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
            "the House departure screen is not ready for UI attachment"
        };
    }

    scene_manager_ = scene;
    scene_generation_ = context.scene_generation;
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
    button_node_ = MewUI_FindNodeInSceneByName(
        scene_manager_,
        kButtonNode);
    marker_text_ =
        MewUI_FindNodeInSceneByName(scene_manager_, kMarkerTextNode);
    if (button_ == nullptr ||
        button_node_ == nullptr ||
        marker_text_ == nullptr) {
        scene_manager_ = nullptr;
        button_ = nullptr;
        button_node_ = nullptr;
        marker_text_ = nullptr;
        scene_generation_ = 0;
        click_handler_ = {};
        return {
            ErrorCode::UiNodeNotFound,
            "the House recommendation button or demo text node is unavailable"
        };
    }

    (void)created;
    ClearAllVisuals();
    MewUI_PlayMovieClipFrame(button_node_, kVisibleFrame);
    MewUI_SetButtonEnabled(button_, 1);
    MewUI_SetButtonInteractable(button_, 1);
    return {};
}

Result<RecommendationVisual>
MewUiRecommendationMarkerView::ShowDemoTextMarker(
    std::uint64_t scene_generation) {
    if (!SceneIsUsable() ||
        marker_text_ == nullptr ||
        scene_generation == 0 ||
        scene_generation != scene_generation_) {
        return {
            {},
            ErrorCode::SceneUnavailable,
            "the generation-bound demo marker target is unavailable"
        };
    }

    if (MewUI_SetTextInSceneFromLocalizationKey(
            scene_manager_,
            kMarkerTextNode,
            kDemoMarkerText) == 0 ||
        MewUI_SetTextInSceneFromLocalizationKey(
            scene_manager_,
            kExplanationTextNode,
            kDemoExplanationText) == 0) {
        ClearAllVisuals();
        return {
            {},
            ErrorCode::UiNodeNotFound,
            "the text-only demo marker could not be rendered"
        };
    }

    return {
        {
            marker_text_,
            nullptr,
            nullptr,
            marker_text_,
            scene_generation_
        },
        ErrorCode::Ok,
        {}
    };
}

void MewUiRecommendationMarkerView::ClearAllVisuals() noexcept {
    if (!SceneIsUsable()) {
        return;
    }
    (void)MewUI_SetTextInSceneFromLocalizationKey(
        scene_manager_,
        kMarkerTextNode,
        kEmptyText);
    (void)MewUI_SetTextInSceneFromLocalizationKey(
        scene_manager_,
        kExplanationTextNode,
        kEmptyText);
}

void MewUiRecommendationMarkerView::Detach() noexcept {
    if (SceneIsUsable() &&
        MewUI_IsComponentInScene(scene_manager_, button_) != 0) {
        MewUI_SetButtonInteractable(button_, 0);
        MewUI_SetButtonEnabled(button_, 0);
        ClearAllVisuals();
        if (button_node_ != nullptr) {
            MewUI_PlayMovieClipFrame(button_node_, kHiddenFrame);
        }
    }
    scene_manager_ = nullptr;
    button_ = nullptr;
    button_node_ = nullptr;
    marker_text_ = nullptr;
    scene_generation_ = 0;
    click_handler_ = {};
}

void MewUiRecommendationMarkerView::SetButtonState(
    RecommendationButtonState state) {
    if (button_ == nullptr) {
        return;
    }

    const char* label = kReadyText;
    bool enabled = true;
    switch (state) {
    case RecommendationButtonState::Hidden:
        enabled = false;
        break;
    case RecommendationButtonState::Ready:
        break;
    case RecommendationButtonState::Marking:
        label = kMarkingText;
        enabled = false;
        break;
    case RecommendationButtonState::Marked:
        label = kMarkedText;
        break;
    }
    MewUI_SetButtonLabelFromLocalizationKey(button_, label);
    MewUI_SetButtonInteractable(button_, enabled ? 1 : 0);
    MewUI_SetButtonEnabled(button_, enabled ? 1 : 0);
}

bool MewUiRecommendationMarkerView::IsAttached() const noexcept {
    return scene_manager_ != nullptr &&
           button_ != nullptr &&
           marker_text_ != nullptr;
}

void* MewUiRecommendationMarkerView::SafeTestTarget() const noexcept {
    return marker_text_;
}

bool MewUiRecommendationMarkerView::SceneIsUsable() const noexcept {
    return scene_manager_ != nullptr &&
           MewUI_IsSceneDestroying(scene_manager_) == 0;
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
    auto* self = static_cast<MewUiRecommendationMarkerView*>(user_data);
    if (self != nullptr &&
        event_type == MEW_BUTTON_EVENT_CLICK &&
        self->click_handler_) {
        self->click_handler_();
    }
}

}  // namespace autocattery::ui
