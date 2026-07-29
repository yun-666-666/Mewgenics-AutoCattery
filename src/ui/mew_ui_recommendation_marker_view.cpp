#include "mew_ui_recommendation_marker_view.hpp"

#include <array>
#include <string>
#include <utility>

namespace autocattery::ui {
namespace {

constexpr auto kButtonNode = "recommend_button";
constexpr auto kButtonRole =
    "AutoCattery.Recommendation.MarkCombatCatsButton";
constexpr auto kReadyText = "HOUSE.RECOMMEND_COMBAT_CATS";
constexpr auto kMarkedText = "HOUSE.RECOMMEND_CLEAR";
constexpr std::array<const char*, 8> kItemNodes{
    "recommend_cat_1",
    "recommend_cat_2",
    "recommend_cat_3",
    "recommend_cat_4",
    "recommend_cat_5",
    "recommend_cat_6",
    "recommend_cat_7",
    "recommend_cat_8"
};
constexpr std::array<const char*, 8> kItemRoles{
    "AutoCattery.Recommendation.Cat1",
    "AutoCattery.Recommendation.Cat2",
    "AutoCattery.Recommendation.Cat3",
    "AutoCattery.Recommendation.Cat4",
    "AutoCattery.Recommendation.Cat5",
    "AutoCattery.Recommendation.Cat6",
    "AutoCattery.Recommendation.Cat7",
    "AutoCattery.Recommendation.Cat8"
};

}  // namespace

Result<void> MewUiRecommendationMarkerView::Attach(
    const UiContextSnapshot& context,
    ClickHandler click_handler,
    ItemClickHandler item_click_handler) {
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
        item_click_handler_ = std::move(item_click_handler);
        active_ = true;
        MewUI_SetButtonEnabled(button_, 1);
        MewUI_SetButtonInteractable(button_, 1);
        return {};
    }

    scene_manager_ = scene;
    button_ = nullptr;
    active_ = false;
    click_handler_ = std::move(click_handler);
    item_click_handler_ = std::move(item_click_handler);
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

    for (std::size_t index = 0; index < item_buttons_.size(); ++index) {
        auto* item_node =
            MewUI_FindNodeInSceneByName(scene_manager_, kItemNodes[index]);
        MewButtonCreateInfo item_info{};
        item_info.scene_manager = scene_manager_;
        item_info.button_node = item_node;
        item_info.node_name = kItemNodes[index];
        item_info.role_name = kItemRoles[index];
        item_info.label_text = "";
        item_info.enabled = 1;
        item_info.activate_enabled = 1;
        item_info.strict_mouse = 1;
        item_info.interact_override = MEW_BUTTON_INTERACT_FORCE_ENABLED;
        item_info.callback = &ButtonCallback;
        item_info.user_data = this;
        item_buttons_[index] = MewUI_CreateButtonFromNode(&item_info);
        if (item_buttons_[index] == nullptr) {
            Detach();
            return {
                ErrorCode::UiNodeNotFound,
                "a dedicated recommendation item asset is unavailable"
            };
        }
    }

    active_ = true;
    MewUI_SetButtonEnabled(button_, 1);
    MewUI_SetButtonInteractable(button_, 1);
    ClearSummary();
    return {};
}

void MewUiRecommendationMarkerView::Detach() noexcept {
    ClearSummary();
    if (scene_manager_ != nullptr &&
        button_ != nullptr &&
        MewUI_IsSceneDestroying(scene_manager_) == 0 &&
        MewUI_IsComponentInScene(scene_manager_, button_) != 0) {
        MewUI_SetButtonInteractable(button_, 0);
        MewUI_SetButtonEnabled(button_, 0);
    } else {
        scene_manager_ = nullptr;
        button_ = nullptr;
        item_buttons_.fill(nullptr);
    }
    active_ = false;
    click_handler_ = {};
    item_click_handler_ = {};
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
    if (status == RecommendationUiStatus::Marked) {
        MewUI_SetButtonLabelFromLocalizationKey(button_, kMarkedText);
        return;
    }
    MewUI_SetButtonLabelFromLocalizationKey(button_, kReadyText);
}

Result<void> MewUiRecommendationMarkerView::ShowItems(
    const std::vector<std::string>& labels) {
    if (scene_manager_ == nullptr ||
        labels.empty() ||
        labels.size() > item_buttons_.size()) {
        return {
            ErrorCode::UiNodeNotFound,
            "recommendation item buttons are unavailable"
        };
    }
    for (std::size_t index = 0; index < item_buttons_.size(); ++index) {
        const bool shown = index < labels.size();
        if (shown &&
            MewUI_SetButtonLabelText(
                item_buttons_[index],
                labels[index].c_str()) == 0) {
            ClearSummary();
            return {
                ErrorCode::UiNodeNotFound,
                "recommendation item label is unavailable"
            };
        }
        MewUI_SetButtonEnabled(item_buttons_[index], shown ? 1 : 0);
        MewUI_SetButtonInteractable(item_buttons_[index], shown ? 1 : 0);
    }
    return {};
}

void MewUiRecommendationMarkerView::ClearSummary() noexcept {
    if (scene_manager_ == nullptr ||
        MewUI_IsSceneDestroying(scene_manager_) != 0) {
        return;
    }
    for (auto* item : item_buttons_) {
        if (item != nullptr &&
            MewUI_IsComponentInScene(scene_manager_, item) != 0) {
            MewUI_ClearButtonLabel(item);
            MewUI_SetButtonInteractable(item, 0);
            MewUI_SetButtonEnabled(item, 0);
        }
    }
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
        event_type == MEW_BUTTON_EVENT_CLICK) {
        if (button == self->button_ && self->click_handler_) {
            self->click_handler_();
            return;
        }
        for (std::size_t index = 0;
             index < self->item_buttons_.size();
             ++index) {
            if (button == self->item_buttons_[index] &&
                self->item_click_handler_) {
                self->item_click_handler_(index);
                return;
            }
        }
    }
}

}  // namespace autocattery::ui
