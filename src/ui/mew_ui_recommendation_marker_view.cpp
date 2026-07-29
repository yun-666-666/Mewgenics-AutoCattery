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
constexpr std::array<const char*, 4> kItemNodes{
    "recommend_cat_1",
    "recommend_cat_2",
    "recommend_cat_3",
    "recommend_cat_4"
};
constexpr std::array<const char*, 4> kItemRoles{
    "AutoCattery.Recommendation.Cat1",
    "AutoCattery.Recommendation.Cat2",
    "AutoCattery.Recommendation.Cat3",
    "AutoCattery.Recommendation.Cat4"
};
MewUiRecommendationMarkerView* g_wheel_view{};

}  // namespace

MewUiRecommendationMarkerView::~MewUiRecommendationMarkerView() {
    Detach();
}

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
        if (!InstallWheelHook()) {
            return {
                ErrorCode::InternalError,
                "mouse-wheel observation could not be attached"
            };
        }
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
        item_nodes_[index] = item_node;
        MewButtonCreateInfo item_info{};
        item_info.scene_manager = scene_manager_;
        item_info.button_node = item_node;
        item_info.node_name = kItemNodes[index];
        item_info.role_name = kItemRoles[index];
        item_info.label_text = "";
        item_info.enabled = 0;
        item_info.activate_enabled = 0;
        item_info.strict_mouse = 1;
        item_info.interact_override = MEW_BUTTON_INTERACT_FORCE_DISABLED;
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

    if (!InstallWheelHook()) {
        Detach();
        return {
            ErrorCode::InternalError,
            "mouse-wheel observation could not be attached"
        };
    }
    active_ = true;
    MewUI_SetButtonEnabled(button_, 1);
    MewUI_SetButtonInteractable(button_, 1);
    ClearSummary();
    return {};
}

void MewUiRecommendationMarkerView::Detach() noexcept {
    ClearSummary();
    RemoveWheelHook();
    if (scene_manager_ != nullptr &&
        button_ != nullptr &&
        MewUI_IsSceneDestroying(scene_manager_) == 0 &&
        MewUI_IsComponentInScene(scene_manager_, button_) != 0) {
        MewUI_SetButtonInteractable(button_, 0);
        MewUI_SetButtonEnabled(button_, 0);
    } else {
        scene_manager_ = nullptr;
        button_ = nullptr;
        item_nodes_.fill(nullptr);
        item_buttons_.fill(nullptr);
    }
    active_ = false;
    hovered_item_.store(-1);
    pending_wheel_delta_.store(0);
    wheel_delta_remainder_ = 0;
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
        labels.empty()) {
        return {
            ErrorCode::UiNodeNotFound,
            "recommendation item buttons are unavailable"
        };
    }
    item_labels_ = labels;
    first_visible_item_ = 0;
    if (!RefreshVisibleItems()) {
        ClearSummary();
        return {
            ErrorCode::UiNodeNotFound,
            "recommendation item label is unavailable"
        };
    }
    return {};
}

void MewUiRecommendationMarkerView::ClearSummary() noexcept {
    item_labels_.clear();
    first_visible_item_ = 0;
    hovered_item_.store(-1);
    pending_wheel_delta_.store(0);
    wheel_delta_remainder_ = 0;
    if (scene_manager_ == nullptr ||
        MewUI_IsSceneDestroying(scene_manager_) != 0) {
        return;
    }
    for (std::size_t row = 0; row < item_buttons_.size(); ++row) {
        auto* item = item_buttons_[row];
        if (item != nullptr &&
            MewUI_IsComponentInScene(scene_manager_, item) != 0) {
            MewUI_PlayMovieClipFrame(item_nodes_[row], 59);
            MewUI_SetButtonInteractable(item, 0);
            MewUI_SetButtonEnabled(item, 0);
            MewUI_ClearButtonLabel(item);
        }
    }
}

void MewUiRecommendationMarkerView::Poll() {
    const int delta = pending_wheel_delta_.exchange(0);
    if (!active_ || item_labels_.size() <= item_buttons_.size() ||
        hovered_item_.load() < 0 || delta == 0) {
        return;
    }
    wheel_delta_remainder_ += delta;
    const int steps = wheel_delta_remainder_ / WHEEL_DELTA;
    wheel_delta_remainder_ %= WHEEL_DELTA;
    if (steps == 0) {
        return;
    }

    const auto max_first =
        item_labels_.size() - item_buttons_.size();
    if (steps > 0) {
        const auto up = static_cast<std::size_t>(steps);
        first_visible_item_ =
            up > first_visible_item_ ? 0 : first_visible_item_ - up;
    } else {
        const auto down = static_cast<std::size_t>(-steps);
        first_visible_item_ =
            (down > max_first - first_visible_item_)
                ? max_first
                : first_visible_item_ + down;
    }
    RefreshVisibleItems();
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
                self->item_click_handler_(
                    self->first_visible_item_ + index);
                return;
            }
        }
    }
    if (self == nullptr) {
        return;
    }
    for (std::size_t index = 0;
         index < self->item_buttons_.size();
         ++index) {
        if (button != self->item_buttons_[index]) {
            continue;
        }
        if (event_type == MEW_BUTTON_EVENT_HOVER_ENTER) {
            self->hovered_item_.store(static_cast<int>(index));
        } else if (event_type == MEW_BUTTON_EVENT_HOVER_EXIT &&
                   self->hovered_item_.load() ==
                       static_cast<int>(index)) {
            self->hovered_item_.store(-1);
        }
        return;
    }
}

LRESULT CALLBACK MewUiRecommendationMarkerView::WheelMessageHook(
    int code,
    WPARAM remove_message,
    LPARAM message_pointer) {
    if (code >= 0 && remove_message == PM_REMOVE &&
        g_wheel_view != nullptr &&
        g_wheel_view->active_ &&
        g_wheel_view->hovered_item_.load() >= 0) {
        const auto* message =
            reinterpret_cast<const MSG*>(message_pointer);
        if (message != nullptr &&
            message->message == WM_MOUSEWHEEL) {
            const auto delta =
                GET_WHEEL_DELTA_WPARAM(message->wParam);
            g_wheel_view->pending_wheel_delta_.fetch_add(delta);
        }
    }
    return CallNextHookEx(
        nullptr,
        code,
        remove_message,
        message_pointer);
}

bool MewUiRecommendationMarkerView::InstallWheelHook() noexcept {
    if (wheel_hook_ != nullptr) {
        return true;
    }
    if (g_wheel_view != nullptr && g_wheel_view != this) {
        return false;
    }
    g_wheel_view = this;
    wheel_hook_ = SetWindowsHookExW(
        WH_GETMESSAGE,
        &WheelMessageHook,
        nullptr,
        GetCurrentThreadId());
    if (wheel_hook_ == nullptr) {
        g_wheel_view = nullptr;
        return false;
    }
    return true;
}

void MewUiRecommendationMarkerView::RemoveWheelHook() noexcept {
    if (wheel_hook_ != nullptr) {
        UnhookWindowsHookEx(wheel_hook_);
        wheel_hook_ = nullptr;
    }
    if (g_wheel_view == this) {
        g_wheel_view = nullptr;
    }
}

bool MewUiRecommendationMarkerView::RefreshVisibleItems() noexcept {
    if (scene_manager_ == nullptr ||
        MewUI_IsSceneDestroying(scene_manager_) != 0) {
        return false;
    }
    for (std::size_t row = 0; row < item_buttons_.size(); ++row) {
        auto* item = item_buttons_[row];
        if (item == nullptr ||
            MewUI_IsComponentInScene(scene_manager_, item) == 0) {
            return false;
        }
        const auto item_index = first_visible_item_ + row;
        const bool shown = item_index < item_labels_.size();
        if (shown &&
            MewUI_PlayMovieClipFrame(item_nodes_[row], 0) == 0) {
            return false;
        }
        if (shown &&
            MewUI_SetButtonLabelText(
                item,
                item_labels_[item_index].c_str()) == 0) {
            return false;
        }
        MewUI_SetButtonEnabled(item, shown ? 1 : 0);
        MewUI_SetButtonInteractable(item, shown ? 1 : 0);
        if (!shown) {
            MewUI_ClearButtonLabel(item);
        }
    }
    return true;
}

}  // namespace autocattery::ui
