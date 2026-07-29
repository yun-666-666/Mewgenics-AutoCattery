#pragma once

#include "auto_cattery/ui/recommendation_marker_controller.hpp"
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#include "mew_ui_api.h"

namespace autocattery::ui {

class MewUiRecommendationMarkerView final
    : public RecommendationMarkerView {
public:
    Result<void> Attach(
        const UiContextSnapshot& context,
        ClickHandler click_handler) override;
    void Detach() noexcept override;
    void SetStatus(RecommendationUiStatus status) override;
    Result<void> ShowSummary(std::string_view summary) override;
    void ClearSummary() noexcept override;
    [[nodiscard]] bool IsAttached() const noexcept override;

private:
    static void __cdecl ButtonCallback(
        void* button,
        MewButtonEvent event_type,
        MewButtonState old_state,
        MewButtonState new_state,
        void* user_data);

    void* scene_manager_{};
    void* button_{};
    bool active_{};
    ClickHandler click_handler_;
};

}  // namespace autocattery::ui
