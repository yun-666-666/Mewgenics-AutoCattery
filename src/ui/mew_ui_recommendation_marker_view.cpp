#include "mew_ui_recommendation_marker_view.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <string>
#include <utility>

#include "mew_ui_movie_clip.hpp"
#include "mew_ui_safe_node_lookup.hpp"

namespace autocattery::ui {
namespace {

constexpr auto kButtonNode = "recommend_button";
constexpr auto kButtonRole =
    "AutoCattery.Recommendation.MarkCombatCatsButton";
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
struct HitRectangle {
    double left;
    double top;
    double right;
    double bottom;
};
constexpr std::array<HitRectangle, 4> kItemHitRectangles{{
    {975.0, 170.0, 1110.0, 220.0},
    {1115.0, 170.0, 1250.0, 220.0},
    {975.0, 230.0, 1110.0, 280.0},
    {1115.0, 230.0, 1250.0, 280.0}
}};
constexpr auto kClickAnimationDuration =
    std::chrono::milliseconds(90);
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

    scene_manager_ = scene;
    button_ = nullptr;
    attached_generation_ = context.scene_generation;
    item_nodes_.fill(nullptr);
    item_text_nodes_.fill(nullptr);
    active_ = false;
    click_handler_ = std::move(click_handler);
    item_click_handler_ = std::move(item_click_handler);
    const auto match = FindSceneUiNode(scene_manager_, kButtonNode);
    auto* button_node = match.node;
    root_node_ = match.root;
    if (root_node_ == nullptr || button_node == nullptr) {
        ResetSceneState();
        return {
            ErrorCode::UiNodeNotFound,
            "the dedicated recommendation button asset is unavailable"
        };
    }
    MewButtonCreateInfo create_info{};
    create_info.scene_manager = scene_manager_;
    create_info.root_node = root_node_;
    create_info.button_node = button_node;
    create_info.node_name = kButtonNode;
    create_info.role_name = kButtonRole;
    create_info.label_text = furniture_mode_
        ? (english_ ? "Start Analysis" : "开始分析")
        : (english_ ? "Mark Combat Cats" : "标记推荐战斗猫");
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
        scene_manager_ = nullptr;
        click_handler_ = {};
        return {
            ErrorCode::UiNodeNotFound,
            "the dedicated recommendation button asset is unavailable"
        };
    }
    MewUI_RegisterExistingButton(
        button_,
        kButtonRole,
        &ButtonCallback,
        this);

    if (!ResolveItemNodes()) {
        Detach();
        return {
            ErrorCode::UiNodeNotFound,
            "a static recommendation row asset is unavailable"
        };
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
    RemoveWheelHook();
    const bool can_touch_scene = CanTouchScene();
    if (can_touch_scene) {
        ClearSummary();
    }
    if (can_touch_scene && button_ != nullptr &&
        MewUI_IsComponentInScene(scene_manager_, button_) != 0) {
        MewUI_SetButtonInteractable(button_, 0);
        MewUI_SetButtonEnabled(button_, 0);
    }
    ResetSceneState();
}

void MewUiRecommendationMarkerView::AbandonScene() noexcept {
    RemoveWheelHook();
    if (button_ != nullptr) {
        if (auto* record = MewUI_GetButtonRecord(button_); record != nullptr) {
            std::memset(record, 0, sizeof(*record));
        }
    }
    ResetSceneState();
}

void MewUiRecommendationMarkerView::ResetSceneState() noexcept {
    active_ = false;
    scene_manager_ = nullptr;
    root_node_ = nullptr;
    button_ = nullptr;
    attached_generation_ = 0;
    item_nodes_.fill(nullptr);
    item_text_nodes_.fill(nullptr);
    availability_applied_ = false;
    pending_press_row_.store(-1);
    pending_click_row_.store(-1);
    pending_wheel_delta_.store(0);
    wheel_delta_remainder_ = 0;
    pressed_row_ = -1;
    pending_activation_item_.reset();
    click_handler_ = {};
    item_click_handler_ = {};
}

void MewUiRecommendationMarkerView::SetAvailable(bool available) {
    if (availability_applied_ && available_ == available) return;
    available_ = available;
    if (button_ == nullptr || !CanTouchScene()) {
        availability_applied_ = false;
        return;
    }
    MewUI_SetButtonInteractable(button_, available ? 1 : 0);
    MewUI_SetButtonEnabled(button_, available ? 1 : 0);
    if (!available) ClearSummary();
    availability_applied_ = true;
}

void MewUiRecommendationMarkerView::SetStatus(
    RecommendationUiStatus status) {
    current_status_ = status;
    if (button_ == nullptr) {
        return;
    }
    if (furniture_mode_) {
        if (status == RecommendationUiStatus::ProbeRequired) {
            MewUI_SetButtonLabelText(
                button_, english_ ? "Analyzing..." : "正在分析...");
            return;
        }
        if (status == RecommendationUiStatus::Marked) {
            MewUI_SetButtonLabelText(
                button_, english_ ? "Analyze Again" : "重新分析");
            return;
        }
        MewUI_SetButtonLabelText(
            button_, english_ ? "Start Analysis" : "开始分析");
        return;
    }
    if (status == RecommendationUiStatus::ProbeRequired) {
        MewUI_SetButtonLabelText(
            button_, english_ ? "Generating..." : "正在生成推荐...");
        return;
    }
    if (status == RecommendationUiStatus::Marked) {
        MewUI_SetButtonLabelText(
            button_, english_ ? "Clear Recommendations" : "清除推荐标记");
        return;
    }
    MewUI_SetButtonLabelText(
        button_, english_ ? "Mark Combat Cats" : "标记推荐战斗猫");
}

void MewUiRecommendationMarkerView::SetFurnitureMode(
    bool furniture_mode) {
    if (furniture_mode_ == furniture_mode) return;
    furniture_mode_ = furniture_mode;
    if (button_ != nullptr && CanTouchScene()) {
        SetStatus(current_status_);
    }
}

void MewUiRecommendationMarkerView::SetEnglish(bool english) {
    english_ = english;
    if (button_ != nullptr && CanTouchScene()) SetStatus(current_status_);
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
    pending_press_row_.store(-1);
    pending_click_row_.store(-1);
    pending_wheel_delta_.store(0);
    wheel_delta_remainder_ = 0;
    pressed_row_ = -1;
    pending_activation_item_.reset();
    if (!CanTouchScene()) {
        return;
    }
    for (std::size_t row = 0; row < item_nodes_.size(); ++row) {
        if (item_nodes_[row] != nullptr &&
            item_text_nodes_[row] != nullptr) {
            MewUI_SetTextElementText(item_text_nodes_[row], "");
            HoldMewUiMovieClipFrame(item_nodes_[row], 0);
        }
    }
}

void MewUiRecommendationMarkerView::Poll() {
    const auto now = std::chrono::steady_clock::now();
    const int pressed_row = pending_press_row_.exchange(-1);
    const int clicked_row = pending_click_row_.exchange(-1);

    if (active_ && pressed_row >= 0 &&
        static_cast<std::size_t>(pressed_row) < item_nodes_.size()) {
        const auto item_index =
            first_visible_item_ + static_cast<std::size_t>(pressed_row);
        if (item_index < item_labels_.size() &&
            HoldMewUiMovieClipFrame(item_nodes_[pressed_row], 2)) {
            pressed_row_ = pressed_row;
            pressed_until_ = now + kClickAnimationDuration;
        }
    }

    if (active_ && clicked_row >= 0 &&
        static_cast<std::size_t>(clicked_row) < item_nodes_.size()) {
        const auto item_index =
            first_visible_item_ + static_cast<std::size_t>(clicked_row);
        if (item_index < item_labels_.size() && item_click_handler_) {
            pending_activation_item_ = item_index;
            HoldMewUiMovieClipFrame(item_nodes_[clicked_row], 2);
            pressed_row_ = clicked_row;
            pressed_until_ = now + kClickAnimationDuration;
        }
    }

    if (active_ && pressed_row_ >= 0 &&
        now >= pressed_until_) {
        const auto row = static_cast<std::size_t>(pressed_row_);
        const auto visible_index = first_visible_item_ + row;
        if (visible_index < item_labels_.size()) {
            HoldMewUiMovieClipFrame(item_nodes_[row], 1);
        }
        pressed_row_ = -1;
        auto activation = std::exchange(
            pending_activation_item_,
            std::nullopt);
        if (activation.has_value() && item_click_handler_) {
            item_click_handler_(*activation);
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
    pressed_row_ = -1;
    pending_activation_item_.reset();
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
        self->available_ &&
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
        g_wheel_view->active_ &&
        g_wheel_view->available_) {
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
                       message->message == WM_LBUTTONDOWN) {
                g_wheel_view->pending_press_row_.store(row);
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

bool MewUiRecommendationMarkerView::ResolveItemNodes() noexcept {
    if (!CanTouchScene()) {
        return false;
    }
    for (std::size_t index = 0; index < item_nodes_.size(); ++index) {
        item_nodes_[index] = MewUI_FindChildByName(
            root_node_, kItemNodes[index]);
        item_text_nodes_[index] = MewUI_FindChildByName(
            root_node_, kItemTextNodes[index]);
        if (item_nodes_[index] == nullptr ||
            item_text_nodes_[index] == nullptr) {
            item_nodes_.fill(nullptr);
            item_text_nodes_.fill(nullptr);
            return false;
        }
    }
    return true;
}

bool MewUiRecommendationMarkerView::CanTouchScene() const noexcept {
    if (scene_manager_ == nullptr ||
        MewUI_GetSceneByName("House") != scene_manager_) {
        return false;
    }
    return MewUI_IsSceneReadyForUITick(scene_manager_) != 0 &&
           MewUI_IsSceneDestroying(scene_manager_) == 0;
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
    if (!CanTouchScene()) {
        return false;
    }
    for (std::size_t row = 0; row < item_nodes_.size(); ++row) {
        if (item_nodes_[row] == nullptr) {
            return false;
        }
        const auto item_index = first_visible_item_ + row;
        const bool shown = item_index < item_labels_.size();
        if (item_text_nodes_[row] == nullptr ||
            MewUI_SetTextElementText(
                item_text_nodes_[row],
                shown ? item_labels_[item_index].c_str() : "") == 0) {
            return false;
        }
        if (!HoldMewUiMovieClipFrame(
                item_nodes_[row],
                shown ? 1 : 0)) {
            return false;
        }
    }
    return true;
}

int MewUiRecommendationMarkerView::HitTestRow(
    HWND window) const noexcept {
    if (window == nullptr || !active_ || !available_ || item_labels_.empty()) {
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
    const auto visible_rows =
        std::min(item_nodes_.size(), item_labels_.size());
    for (std::size_t row = 0; row < visible_rows; ++row) {
        const auto& rectangle = kItemHitRectangles[row];
        if (virtual_x >= rectangle.left &&
            virtual_x <= rectangle.right &&
            virtual_y >= rectangle.top &&
            virtual_y <= rectangle.bottom) {
            return static_cast<int>(row);
        }
    }
    return -1;
}

}  // namespace autocattery::ui
