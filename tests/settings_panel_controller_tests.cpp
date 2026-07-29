#include "auto_cattery/ui/settings_panel_controller.hpp"

#include <span>
#include <string>
#include <vector>

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

class FakeSettingsPanelView final : public ui::SettingsPanelView {
public:
    Result<void> Attach(const ui::UiContextSnapshot&) override {
        attached = true;
        return {};
    }

    void Detach() noexcept override {
        attached = false;
        controls.clear();
    }

    Result<void> Render(
        std::string_view next_title,
        std::span<const SettingsControl> next_controls,
        std::size_t next_selected_row) override {
        if (!attached) {
            return {ErrorCode::SceneUnavailable, "not attached"};
        }
        title = next_title;
        controls.assign(next_controls.begin(), next_controls.end());
        selected_row = next_selected_row;
        ++render_count;
        return {};
    }

    [[nodiscard]] bool IsAttached() const noexcept override {
        return attached;
    }

    bool attached{};
    std::string title;
    std::vector<SettingsControl> controls;
    std::size_t selected_row{};
    std::size_t render_count{};
};

}  // namespace

void RunSettingsPanelControllerTests() {
    RuntimeConfigService runtime({}, {});
    AC_CHECK(runtime.LoadInitial().status == ConfigReloadStatus::Applied);
    SettingsService settings(runtime);
    FakeSettingsPanelView view;
    ui::SettingsPanelController controller(settings, view);
    ui::UiContextSnapshot context;
    context.kind = ui::UiContextKind::House;
    context.scene_name = "House";
    context.scene_generation = 3;
    context.input_enabled = true;

    AC_CHECK(static_cast<bool>(controller.Attach(context)));
    AC_CHECK(controller.IsOpen());
    AC_CHECK(controller.PageIndex() == 0);
    AC_CHECK(view.controls.size() == 4);
    AC_CHECK(view.selected_row == 0);

    AC_CHECK(static_cast<bool>(controller.AdjustSelected(
        1,
        workflow::WorkflowState::Idle)));
    AC_CHECK(runtime.Current().combat_scoring.recommended_count == 9);

    AC_CHECK(static_cast<bool>(controller.MoveSelection(1)));
    AC_CHECK(controller.SelectedIndex() == 1);
    AC_CHECK(static_cast<bool>(controller.ActivateSelected(
        workflow::WorkflowState::Idle)));
    AC_CHECK(runtime.Current().combat_scoring.exclude_injured);

    AC_CHECK(static_cast<bool>(controller.NextPage(1)));
    AC_CHECK(controller.PageIndex() == 1);
    AC_CHECK(view.controls.size() == 4);
    AC_CHECK(static_cast<bool>(controller.AdjustSelected(
        1,
        workflow::WorkflowState::Idle)));
    AC_CHECK(runtime.Current().combat_scoring.stat_weights[0] == 1.25);
    for (int index = 0; index < 4; ++index) {
        AC_CHECK(static_cast<bool>(controller.MoveSelection(1)));
    }
    AC_CHECK(controller.SelectedIndex() == 4);
    AC_CHECK(view.selected_row == 0);

    AC_CHECK(static_cast<bool>(controller.NextPage(1)));
    AC_CHECK(controller.PageIndex() == 2);
    AC_CHECK(static_cast<bool>(controller.AdjustSelected(
        -1,
        workflow::WorkflowState::Idle)));
    AC_CHECK(!runtime.Current().execution_safety.read_only_mode);

    AC_CHECK(static_cast<bool>(controller.MoveSelection(1)));
    AC_CHECK(static_cast<bool>(controller.AdjustSelected(
        -1,
        workflow::WorkflowState::Idle)));
    AC_CHECK(!runtime.Current().execution_safety.create_backup_before_apply);
    AC_CHECK(!runtime.Current().execution.cull_enabled);

    AC_CHECK(static_cast<bool>(controller.MoveSelection(1)));
    AC_CHECK(static_cast<bool>(controller.ActivateSelected(
        workflow::WorkflowState::Idle)));
    AC_CHECK(!runtime.Current().execution_safety.single_click_execute);
    AC_CHECK(view.title.find("Confirm") != std::string::npos);
    AC_CHECK(static_cast<bool>(controller.ActivateSelected(
        workflow::WorkflowState::Idle)));
    AC_CHECK(runtime.Current().execution_safety.single_click_execute);

    AC_CHECK(static_cast<bool>(controller.MoveSelection(1)));
    AC_CHECK(static_cast<bool>(controller.ActivateSelected(
        workflow::WorkflowState::Idle)));
    AC_CHECK(view.title.find("Locked") != std::string::npos);
    AC_CHECK(runtime.Current().execution_safety
        .require_preview_before_destructive_actions);

    AC_CHECK(static_cast<bool>(controller.NextPage(1)));
    AC_CHECK(controller.PageIndex() == 0);
    const auto deferred = controller.AdjustSelected(
        1,
        workflow::WorkflowState::Scoring);
    AC_CHECK(!static_cast<bool>(deferred));
    AC_CHECK(deferred.code == ErrorCode::WriteConflict);
    AC_CHECK(runtime.Current().combat_scoring.recommended_count == 9);
    AC_CHECK(view.title.find("Pending") != std::string::npos);
    AC_CHECK(runtime.PollHotReload(workflow::WorkflowState::Idle).status ==
             ConfigReloadStatus::Applied);
    AC_CHECK(runtime.Current().combat_scoring.recommended_count == 10);
    AC_CHECK(static_cast<bool>(controller.Refresh()));

    controller.Detach();
    AC_CHECK(!controller.IsOpen());
    AC_CHECK(!view.attached);
}

}  // namespace autocattery::tests
