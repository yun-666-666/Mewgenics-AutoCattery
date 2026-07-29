#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

#include "auto_cattery/settings_service.hpp"
#include "auto_cattery/ui/scene_context.hpp"

namespace autocattery::ui {

class SettingsPanelView {
public:
    virtual ~SettingsPanelView() = default;
    virtual Result<void> Attach(const UiContextSnapshot& context) = 0;
    virtual void Detach() noexcept = 0;
    virtual Result<void> Render(
        std::string_view title,
        std::span<const SettingsControl> controls,
        std::size_t selected_row) = 0;
    [[nodiscard]] virtual bool IsAttached() const noexcept = 0;
};

class SettingsPanelController final {
public:
    SettingsPanelController(
        SettingsService& settings,
        SettingsPanelView& view);

    [[nodiscard]] Result<void> Attach(const UiContextSnapshot& context);
    void Detach() noexcept;
    [[nodiscard]] Result<void> Refresh();
    [[nodiscard]] Result<void> NextPage(int direction);
    [[nodiscard]] Result<void> MoveSelection(int direction);
    [[nodiscard]] Result<void> SelectVisibleRow(std::size_t row);
    [[nodiscard]] Result<void> AdjustSelected(
        int direction,
        workflow::WorkflowState workflow_state);
    [[nodiscard]] Result<void> ActivateSelected(
        workflow::WorkflowState workflow_state);

    [[nodiscard]] bool IsOpen() const noexcept;
    [[nodiscard]] std::size_t PageIndex() const noexcept;
    [[nodiscard]] std::size_t SelectedIndex() const noexcept;

private:
    [[nodiscard]] Result<void> ApplySelected(
        int direction,
        bool toggle,
        workflow::WorkflowState workflow_state);
    [[nodiscard]] Result<void> RenderCurrent();
    void RecordApplyResult(const Result<void>& result);

    SettingsService& settings_;
    SettingsPanelView& view_;
    std::size_t page_index_{};
    std::size_t selected_index_{};
    bool open_{};
    bool single_click_confirmation_pending_{};
    std::string status_;
};

}  // namespace autocattery::ui
