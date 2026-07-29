#pragma once

#include <array>
#include <optional>

#include "auto_cattery/ui/settings_panel_controller.hpp"
#include <windows.h>

namespace autocattery::ui {

struct SettingsPanelHit {
    std::size_t row{};
    int direction{};
};

class MewUiSettingsPanelView final : public SettingsPanelView {
public:
    ~MewUiSettingsPanelView() override;

    Result<void> Attach(const UiContextSnapshot& context) override;
    void Detach() noexcept override;
    Result<void> Render(
        std::string_view title,
        std::span<const SettingsControl> controls,
        std::size_t selected_row) override;
    [[nodiscard]] bool IsAttached() const noexcept override;
    [[nodiscard]] std::optional<SettingsPanelHit> HitTest(
        HWND window) const noexcept;

private:
    [[nodiscard]] bool CanTouchScene() const noexcept;
    void ClearRows() noexcept;

    void* scene_manager_{};
    std::uint64_t attached_generation_{};
    std::array<void*, 4> row_nodes_{};
    bool active_{};
};

}  // namespace autocattery::ui
