#include "mew_ui_management_panel_view.hpp"

#include <algorithm>
#include <array>

#include <windowsx.h>

#include "auto_cattery/ui/virtual_viewport.hpp"
#include "auto_cattery/ui/management_panel_input.hpp"

namespace autocattery::ui {
namespace {

struct Rect { double left; double top; double right; double bottom; };
constexpr std::array<Rect, 9> kButtons{{
    {165, 82, 355, 120}, {365, 82, 555, 120},
    {565, 82, 755, 120}, {990, 82, 1120, 124},
    {165, 540, 345, 585},
    {355, 540, 535, 585}, {755, 540, 935, 585},
    {945, 540, 1125, 585}, {765, 82, 955, 120}
}};
constexpr std::array<ManagementPanelControl, 9> kButtonControls{
    ManagementPanelControl::SettingsTab,
    ManagementPanelControl::ProtectionTab,
    ManagementPanelControl::PreviewTab,
    ManagementPanelControl::Close,
    ManagementPanelControl::Previous,
    ManagementPanelControl::Next,
    ManagementPanelControl::Apply,
    ManagementPanelControl::Remove,
    ManagementPanelControl::SeniorTab
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
                const POINT client_point{
                    GET_X_LPARAM(message->lParam), GET_Y_LPARAM(message->lParam)};
                const auto hit = g_panel_view->HitTest(
                    message->hwnd, client_point);
                if (hit) {
                    g_panel_view->pending_control_.store(
                        static_cast<int>(hit->control));
                    g_panel_view->pending_row_.store(
                        static_cast<int>(hit->row));
                    g_panel_view->pending_direction_.store(hit->direction);
                }
            } else if (message->message == WM_MOUSEWHEEL &&
                       g_panel_view->page_.load() !=
                           static_cast<int>(ManagementPanelPage::Settings)) {
                const int delta = GET_WHEEL_DELTA_WPARAM(message->wParam);
                g_panel_view->pending_control_.store(
                    static_cast<int>(ManagementPanelControl::Scroll));
                g_panel_view->pending_row_.store(0);
                g_panel_view->pending_direction_.store(delta > 0 ? -1 : 1);
            }
            if (blocked || ShouldConsumePanelMessage(
                    message->message, message->wParam))
                message->message = WM_NULL;
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
MewUiManagementPanelView::HitTest(HWND window, POINT client_point) const noexcept {
    if (window == nullptr || !visible_.load()) return std::nullopt;
    RECT client{};
    if (GetClientRect(window, &client) == 0) return std::nullopt;
    const auto width = static_cast<double>(client.right - client.left);
    const auto height = static_cast<double>(client.bottom - client.top);
    const auto point = MapClientToVirtualViewport(
        {static_cast<double>(client_point.x), static_cast<double>(client_point.y)},
        width, height);
    if (!point) return std::nullopt;
    const double x = point->x;
    const double y = point->y;
    const auto page = static_cast<ManagementPanelPage>(page_.load());
    for (std::size_t index = 0; index < kButtons.size(); ++index) {
        if (!Contains(kButtons[index], x, y)) continue;
        if (page == ManagementPanelPage::Settings && index >= 4 && index <= 7)
            continue;
        if ((index == 4 || index == 5) && !navigation_visible_.load())
            return std::nullopt;
        if (index == 6 && !apply_visible_.load()) return std::nullopt;
        if (index == 7 && !remove_visible_.load()) return std::nullopt;
        return HitResult{kButtonControls[index], 0, 0};
    }
    if (page != ManagementPanelPage::Settings) {
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
        if ((page == ManagementPanelPage::Protection || page == ManagementPanelPage::Senior) &&
            Contains({160, 445, 610, 493}, x, y))
            return HitResult{ManagementPanelControl::Row, 10, 0};
        if ((page == ManagementPanelPage::Protection || page == ManagementPanelPage::Senior) &&
            Contains({640, 445, 1090, 493}, x, y))
            return HitResult{ManagementPanelControl::Row, 11, 0};
        return std::nullopt;
    }
    const auto setting = HitTestSettingsRow(x, y);
    if (setting) return HitResult{
        setting->begin_edit ? ManagementPanelControl::BeginEdit
                            : ManagementPanelControl::Row,
        setting->row,
        setting->direction};
    return std::nullopt;
}

}  // namespace autocattery::ui
