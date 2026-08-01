#include "mew_ui_management_panel_view.hpp"

#include <algorithm>
#include <array>

namespace autocattery::ui {
namespace {

constexpr double kVirtualWidth = 1280.0;
constexpr double kVirtualHeight = 720.0;
struct Rect { double left; double top; double right; double bottom; };
constexpr std::array<Rect, 7> kButtons{{
    {250, 75, 500, 125}, {530, 75, 780, 125},
    {1000, 75, 1140, 125}, {250, 610, 450, 665},
    {465, 610, 665, 665}, {715, 610, 915, 665},
    {930, 610, 1130, 665}
}};
constexpr std::array<ManagementPanelControl, 7> kButtonControls{
    ManagementPanelControl::SettingsTab,
    ManagementPanelControl::ProtectionTab,
    ManagementPanelControl::Close,
    ManagementPanelControl::Previous,
    ManagementPanelControl::Next,
    ManagementPanelControl::Apply,
    ManagementPanelControl::Remove
};
MewUiManagementPanelView* g_panel_view{};

bool Contains(const Rect& rect, double x, double y) {
    return x >= rect.left && x <= rect.right &&
           y >= rect.top && y <= rect.bottom;
}

}  // namespace

std::optional<ManagementPanelEvent> MewUiManagementPanelView::Poll() {
    const int control = pending_control_.exchange(-1);
    if (control < 0) return std::nullopt;
    ManagementPanelEvent event;
    event.control = static_cast<ManagementPanelControl>(control);
    event.row = static_cast<std::size_t>(
        std::max(0, pending_row_.exchange(-1)));
    event.direction = pending_direction_.exchange(0);
    return event;
}

LRESULT CALLBACK MewUiManagementPanelView::MessageHook(
    int code, WPARAM remove_message, LPARAM message_pointer) {
    if (code >= 0 && remove_message == PM_REMOVE &&
        g_panel_view != nullptr && g_panel_view->visible_.load()) {
        auto* message = reinterpret_cast<MSG*>(message_pointer);
        if (message != nullptr) {
            const bool blocked = message->message == WM_LBUTTONDOWN ||
                message->message == WM_LBUTTONUP ||
                message->message == WM_RBUTTONDOWN ||
                message->message == WM_RBUTTONUP ||
                message->message == WM_MBUTTONDOWN ||
                message->message == WM_MBUTTONUP ||
                message->message == WM_MOUSEWHEEL;
            if (message->message == WM_LBUTTONUP) {
                const auto hit = g_panel_view->HitTest(message->hwnd);
                if (hit) {
                    g_panel_view->pending_control_.store(
                        static_cast<int>(hit->control));
                    g_panel_view->pending_row_.store(
                        static_cast<int>(hit->row));
                    g_panel_view->pending_direction_.store(hit->direction);
                }
            }
            if (blocked) message->message = WM_NULL;
        }
    }
    return CallNextHookEx(nullptr, code, remove_message, message_pointer);
}

bool MewUiManagementPanelView::InstallHook() noexcept {
    if (message_hook_ != nullptr) return true;
    if (g_panel_view != nullptr && g_panel_view != this) return false;
    g_panel_view = this;
    message_hook_ = SetWindowsHookExW(
        WH_GETMESSAGE, &MessageHook, nullptr, GetCurrentThreadId());
    if (message_hook_ == nullptr) {
        g_panel_view = nullptr;
        return false;
    }
    return true;
}

void MewUiManagementPanelView::RemoveHook() noexcept {
    if (message_hook_ != nullptr) {
        UnhookWindowsHookEx(message_hook_);
        message_hook_ = nullptr;
    }
    if (g_panel_view == this) g_panel_view = nullptr;
}

std::optional<MewUiManagementPanelView::HitResult>
MewUiManagementPanelView::HitTest(HWND window) const noexcept {
    if (window == nullptr || !visible_.load()) return std::nullopt;
    POINT cursor{};
    RECT client{};
    if (GetCursorPos(&cursor) == 0 ||
        ScreenToClient(window, &cursor) == 0 ||
        GetClientRect(window, &client) == 0) return std::nullopt;
    const auto width = static_cast<double>(client.right - client.left);
    const auto height = static_cast<double>(client.bottom - client.top);
    if (width <= 0 || height <= 0) return std::nullopt;
    const double x = cursor.x * kVirtualWidth / width;
    const double y = cursor.y * kVirtualHeight / height;
    for (std::size_t index = 0; index < kButtons.size(); ++index) {
        if (Contains(kButtons[index], x, y)) {
            return HitResult{kButtonControls[index], 0, 0};
        }
    }
    for (std::size_t row = 0; row < 8; ++row) {
        const Rect rectangle{210, 145 + row * 55.0,
                             1115, 195 + row * 55.0};
        if (!Contains(rectangle, x, y)) continue;
        int direction = 0;
        if (x < 510) direction = -1;
        else if (x > 815) direction = 1;
        return HitResult{ManagementPanelControl::Row, row, direction};
    }
    return std::nullopt;
}

}  // namespace autocattery::ui
