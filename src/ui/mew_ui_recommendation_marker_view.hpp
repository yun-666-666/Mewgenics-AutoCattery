#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <optional>
#include <vector>

#include "auto_cattery/ui/recommendation_marker_controller.hpp"
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#include "mew_ui_api.h"

namespace autocattery::ui {

class MewUiRecommendationMarkerView final
    : public RecommendationMarkerView {
public:
    ~MewUiRecommendationMarkerView() override;

    Result<void> Attach(
        const UiContextSnapshot& context,
        ClickHandler click_handler,
        ItemClickHandler item_click_handler) override;
    void Detach() noexcept override;
    void SetStatus(RecommendationUiStatus status) override;
    Result<void> ShowItems(
        const std::vector<std::string>& labels) override;
    void ClearSummary() noexcept override;
    void Poll() override;
    [[nodiscard]] bool IsAttached() const noexcept override;

private:
    static void __cdecl ButtonCallback(
        void* button,
        MewButtonEvent event_type,
        MewButtonState old_state,
        MewButtonState new_state,
        void* user_data);
    static LRESULT CALLBACK WheelMessageHook(
        int code,
        WPARAM remove_message,
        LPARAM message_pointer);

    bool InstallWheelHook() noexcept;
    void RemoveWheelHook() noexcept;
    bool RefreshVisibleItems() noexcept;
    [[nodiscard]] int HitTestRow(HWND window) const noexcept;

    void* scene_manager_{};
    void* button_{};
    std::array<void*, 4> item_nodes_{};
    bool active_{};
    HHOOK wheel_hook_{};
    std::atomic<int> pending_press_row_{-1};
    std::atomic<int> pending_click_row_{-1};
    std::atomic<int> pending_wheel_delta_{};
    int wheel_delta_remainder_{};
    int pressed_row_{-1};
    std::chrono::steady_clock::time_point pressed_until_{};
    std::optional<std::size_t> pending_activation_item_;
    std::size_t first_visible_item_{};
    std::vector<std::string> item_labels_;
    ClickHandler click_handler_;
    ItemClickHandler item_click_handler_;
};

}  // namespace autocattery::ui
