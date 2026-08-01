#include "mew_ui_management_panel_view.hpp"

#include <algorithm>
#include <array>

namespace autocattery::ui {
namespace {

constexpr double kVirtualWidth = 1280.0;
constexpr double kVirtualHeight = 720.0;
struct Rect { double left; double top; double right; double bottom; };
constexpr std::array<Rect, 7> kButtons{{
    {165, 82, 355, 120}, {365, 82, 555, 120},
    {990, 82, 1120, 124}, {165, 540, 345, 585},
    {355, 540, 535, 585}, {755, 540, 935, 585},
    {945, 540, 1125, 585}
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
constexpr std::array<std::size_t, 3> kSettingStarts{0, 17, 35};
constexpr std::array<std::size_t, 3> kSettingCounts{17, 18, 10};
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
    event.text = edit_buffer_;
    return event;
}

LRESULT CALLBACK MewUiManagementPanelView::MessageHook(
    int code, WPARAM remove_message, LPARAM message_pointer) {
    if (code >= 0 && remove_message == PM_REMOVE &&
        g_panel_view != nullptr && g_panel_view->visible_.load()) {
        auto* message = reinterpret_cast<MSG*>(message_pointer);
        if (message != nullptr) {
            g_panel_view->CaptureEditMessage(message);
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
            } else if (message->message == WM_MOUSEWHEEL &&
                       g_panel_view->protection_page_.load()) {
                const int delta = GET_WHEEL_DELTA_WPARAM(message->wParam);
                g_panel_view->pending_control_.store(
                    static_cast<int>(ManagementPanelControl::Scroll));
                g_panel_view->pending_row_.store(0);
                g_panel_view->pending_direction_.store(delta > 0 ? -1 : 1);
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
        if (!Contains(kButtons[index], x, y)) continue;
        if (!protection_page_.load() && index >= 3) return std::nullopt;
        if ((index == 3 || index == 4) && !navigation_visible_.load())
            return std::nullopt;
        if (index == 5 && !apply_visible_.load()) return std::nullopt;
        if (index == 6 && !remove_visible_.load()) return std::nullopt;
        return HitResult{kButtonControls[index], 0, 0};
    }
    if (protection_page_.load()) {
        const Rect save{160, 145, 1090, 181};
        if (Contains(save, x, y)) {
            const int direction = x < 470 ? -1 : (x > 780 ? 1 : 0);
            return HitResult{ManagementPanelControl::Row, 0, direction};
        }
        for (std::size_t row = 0; row < 9; ++row) {
            const double left = 160 + (row % 3) * 317.0;
            const double top = 195 + (row / 3) * 80.0;
            const Rect rectangle{left, top, left + 300, top + 62};
            if (!Contains(rectangle, x, y)) continue;
            return HitResult{ManagementPanelControl::Row, row + 1, 0};
        }
        if (Contains({160, 445, 610, 493}, x, y))
            return HitResult{ManagementPanelControl::Row, 10, 0};
        if (Contains({640, 445, 1090, 493}, x, y))
            return HitResult{ManagementPanelControl::Row, 11, 0};
        return std::nullopt;
    }
    for (std::size_t column = 0; column < 3; ++column) {
        for (std::size_t row = 0; row < kSettingCounts[column]; ++row) {
            const double left = 160 + column * 317.0;
            const Rect rectangle{left, 181 + row * 24.0,
                                 left + 300, 204 + row * 24.0};
            if (!Contains(rectangle, x, y)) continue;
            if (x >= left + 75 && x <= left + 225) {
                return HitResult{ManagementPanelControl::BeginEdit,
                                 kSettingStarts[column] + row, 0};
            }
            const int direction = x < left + 75 ? -1 : 1;
            return HitResult{ManagementPanelControl::Row,
                             kSettingStarts[column] + row, direction};
        }
    }
    return std::nullopt;
}

}  // namespace autocattery::ui
