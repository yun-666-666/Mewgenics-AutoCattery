#include "mew_ui_recommendation_marker_view.hpp"

#include <algorithm>
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
constexpr auto kRowText = "HOUSE.RECOMMEND_ROW";
constexpr std::array<const char*, 4> kItemNodes{
    "recommend_row_1",
    "recommend_row_2",
    "recommend_row_3",
    "recommend_row_4"
};
constexpr std::array<const char*, 4> kItemTextNodes{
    "recommend_text_1",
    "recommend_text_2",
    "recommend_text_3",
    "recommend_text_4"
};
constexpr double kVirtualWidth = 1280.0;
constexpr double kVirtualHeight = 720.0;
constexpr double kRowXMin = 995.0;
constexpr double kRowXMax = 1210.0;
constexpr double kRowY = 140.0;
constexpr double kRowStep = 42.0;
constexpr double kRowHeight = 42.0;
constexpr std::size_t kMovieClipStateFlagsOffset = 0x09U;
constexpr unsigned char kMovieClipPlayingBit = 0x02U;
MewUiRecommendationMarkerView* g_wheel_view{};

bool HoldMovieClipFrame(void* movie_clip, int frame) noexcept {
    if (MewUI_PlayMovieClipFrame(movie_clip, frame) == 0) {
        return false;
    }
    __try {
        // Current Mewgenics callers use the same goto-frame function and
        // clear this bit immediately afterward for goto-and-stop behavior.
        auto* flags = static_cast<unsigned char*>(movie_clip) +
            kMovieClipStateFlagsOffset;
        *flags &= static_cast<unsigned char>(~kMovieClipPlayingBit);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

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

    for (std::size_t index = 0; index < item_nodes_.size(); ++index) {
        auto* item_node =
            MewUI_FindNodeInSceneByName(scene_manager_, kItemNodes[index]);
        item_nodes_[index] = item_node;
        auto* text_node = MewUI_FindNodeInSceneByName(
            scene_manager_,
            kItemTextNodes[index]);
        if (item_node == nullptr || text_node == nullptr) {
            Detach();
            return {
                ErrorCode::UiNodeNotFound,
                "a static recommendation row asset is unavailable"
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
    }
    active_ = false;
    pending_click_row_.store(-1);
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
            "recommendation rows are unavailable"
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
    pending_click_row_.store(-1);
    pending_wheel_delta_.store(0);
    wheel_delta_remainder_ = 0;
    if (scene_manager_ == nullptr ||
        MewUI_IsSceneDestroying(scene_manager_) != 0) {
        return;
    }
    for (std::size_t row = 0; row < item_nodes_.size(); ++row) {
        if (item_nodes_[row] != nullptr) {
            MewUI_SetTextInSceneFromLocalizationKeyValue(
                scene_manager_,
                kItemTextNodes[row],
                kRowText,
                "");
            HoldMovieClipFrame(item_nodes_[row], 0);
        }
    }
}

void MewUiRecommendationMarkerView::Poll() {
    const int clicked_row = pending_click_row_.exchange(-1);
    if (active_ && clicked_row >= 0 &&
        static_cast<std::size_t>(clicked_row) < item_nodes_.size()) {
        const auto item_index =
            first_visible_item_ + static_cast<std::size_t>(clicked_row);
        if (item_index < item_labels_.size() && item_click_handler_) {
            item_click_handler_(item_index);
        }
    }

    const int delta = pending_wheel_delta_.exchange(0);
    if (!active_ || item_labels_.size() <= item_nodes_.size() ||
        delta == 0) {
        return;
    }
    wheel_delta_remainder_ += delta;
    const int steps = wheel_delta_remainder_ / WHEEL_DELTA;
    wheel_delta_remainder_ %= WHEEL_DELTA;
    if (steps == 0) {
        return;
    }

    const auto max_first =
        item_labels_.size() - item_nodes_.size();
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
        button == self->button_ &&
        event_type == MEW_BUTTON_EVENT_CLICK &&
        self->click_handler_) {
        self->click_handler_();
    }
}

LRESULT CALLBACK MewUiRecommendationMarkerView::WheelMessageHook(
    int code,
    WPARAM remove_message,
    LPARAM message_pointer) {
    if (code >= 0 && remove_message == PM_REMOVE &&
        g_wheel_view != nullptr &&
        g_wheel_view->active_) {
        const auto* message =
            reinterpret_cast<const MSG*>(message_pointer);
        if (message != nullptr) {
            const int row =
                g_wheel_view->HitTestRow(message->hwnd);
            if (row >= 0 && message->message == WM_MOUSEWHEEL) {
                const auto delta =
                    GET_WHEEL_DELTA_WPARAM(message->wParam);
                g_wheel_view->pending_wheel_delta_.fetch_add(delta);
            } else if (row >= 0 &&
                       message->message == WM_LBUTTONUP) {
                g_wheel_view->pending_click_row_.store(row);
            }
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
    for (std::size_t row = 0; row < item_nodes_.size(); ++row) {
        if (item_nodes_[row] == nullptr) {
            return false;
        }
        const auto item_index = first_visible_item_ + row;
        const bool shown = item_index < item_labels_.size();
        if (MewUI_SetTextInSceneFromLocalizationKeyValue(
                scene_manager_,
                kItemTextNodes[row],
                kRowText,
                shown ? item_labels_[item_index].c_str() : "") == 0) {
            return false;
        }
        if (!HoldMovieClipFrame(
                item_nodes_[row],
                shown ? 1 : 0)) {
            return false;
        }
    }
    return true;
}

int MewUiRecommendationMarkerView::HitTestRow(
    HWND window) const noexcept {
    if (window == nullptr || !active_ || item_labels_.empty()) {
        return -1;
    }

    POINT cursor{};
    RECT client{};
    if (GetCursorPos(&cursor) == 0 ||
        ScreenToClient(window, &cursor) == 0 ||
        GetClientRect(window, &client) == 0) {
        return -1;
    }
    const double client_width =
        static_cast<double>(client.right - client.left);
    const double client_height =
        static_cast<double>(client.bottom - client.top);
    if (client_width <= 0.0 || client_height <= 0.0) {
        return -1;
    }

    const double virtual_x =
        static_cast<double>(cursor.x) *
        kVirtualWidth / client_width;
    const double virtual_y =
        static_cast<double>(cursor.y) *
        kVirtualHeight / client_height;
    if (virtual_x < kRowXMin || virtual_x > kRowXMax) {
        return -1;
    }

    const auto visible_rows =
        std::min(item_nodes_.size(), item_labels_.size());
    for (std::size_t row = 0; row < visible_rows; ++row) {
        const double row_y = kRowY + kRowStep * row;
        if (virtual_y >= row_y &&
            virtual_y <= row_y + kRowHeight) {
            return static_cast<int>(row);
        }
    }
    return -1;
}

}  // namespace autocattery::ui
