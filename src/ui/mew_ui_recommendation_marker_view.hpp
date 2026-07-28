#pragma once

#include "auto_cattery/ui/recommendation_marker_controller.hpp"
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#include "mew_ui_api.h"

namespace autocattery::ui {

class MewUiRecommendationMarkerView final : public RecommendationMarkerView {
public:
    Result<void> Attach(
        const UiContextSnapshot& context,
        ClickHandler click_handler) override;
    Result<RecommendationVisual> ShowDemoTextMarker(
        std::uint64_t scene_generation) override;
    void ClearAllVisuals() noexcept override;
    void Detach() noexcept override;
    void SetButtonState(RecommendationButtonState state) override;
    [[nodiscard]] bool IsAttached() const noexcept override;
    [[nodiscard]] void* SafeTestTarget() const noexcept override;

private:
    static void __cdecl ButtonCallback(
        void* button,
        MewButtonEvent event_type,
        MewButtonState old_state,
        MewButtonState new_state,
        void* user_data);

    [[nodiscard]] bool SceneIsUsable() const noexcept;

    void* scene_manager_{};
    void* button_{};
    void* button_node_{};
    void* marker_text_{};
    std::uint64_t scene_generation_{};
    ClickHandler click_handler_;
};

}  // namespace autocattery::ui
