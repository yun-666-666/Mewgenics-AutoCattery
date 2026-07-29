#pragma once

#include "auto_cattery/ui/house_button_controller.hpp"
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#include "mew_ui_api.h"

namespace autocattery::ui {

class MewUiHouseButtonView final : public HouseButtonView {
public:
    Result<void> Attach(
        const UiContextSnapshot& context,
        ClickHandler click_handler) override;
    void Detach() noexcept override;
    void SetState(
        OrganizeButtonState state,
        std::string_view detail) override;
    void ShowPlaceholder() override;
    [[nodiscard]] bool IsAttached() const noexcept override;

private:
    static void __cdecl ButtonCallback(
        void* button,
        MewButtonEvent event_type,
        MewButtonState old_state,
        MewButtonState new_state,
        void* user_data);
    [[nodiscard]] bool CanTouchScene() const noexcept;

    void* scene_manager_{};
    void* button_{};
    std::uint64_t attached_generation_{};
    bool active_{};
    ClickHandler click_handler_;
};

}  // namespace autocattery::ui
