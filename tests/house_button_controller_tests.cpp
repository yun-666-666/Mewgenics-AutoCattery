#include "auto_cattery/ui/house_button_controller.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <utility>
#include <vector>

#include "auto_cattery/workflow/organize_workflow_facade.hpp"
#include "test_support.hpp"

namespace autocattery::tests {
namespace {

class FakeHouseButtonView final : public ui::HouseButtonView {
public:
    Result<void> Attach(
        const ui::UiContextSnapshot&,
        ClickHandler handler) override {
        ++attach_calls;
        attached = true;
        click_handler = std::move(handler);
        return {};
    }

    void Detach() noexcept override {
        ++detach_calls;
        attached = false;
        click_handler = {};
    }

    void AbandonScene() noexcept override {
        ++abandon_calls;
        attached = false;
        click_handler = {};
    }

    void SetState(
        ui::OrganizeButtonState new_state,
        std::string_view) override {
        state = new_state;
    }

    void ShowPlaceholder() override {
        ++placeholder_calls;
    }

    [[nodiscard]] bool IsAttached() const noexcept override {
        return attached;
    }

    void Click() {
        if (click_handler) {
            click_handler();
        }
    }

    bool attached{};
    int attach_calls{};
    int detach_calls{};
    int abandon_calls{};
    int placeholder_calls{};
    ui::OrganizeButtonState state{ui::OrganizeButtonState::Hidden};
    ClickHandler click_handler;
};

class FakeWorkflow final : public workflow::OrganizeWorkflowFacade {
public:
    Result<void> RequestPreview(
        std::uint64_t scene_generation) override {
        ++preview_calls;
        last_scene_generation = scene_generation;
        return {
            ErrorCode::Ok,
            "no cats were modified"
        };
    }

    workflow::WorkflowCapability CurrentExecutionAvailability()
        const noexcept override {
        return capability;
    }

    Result<workflow::OrganizeOutcome> RequestExecutionOutcome() override {
        ++execution_calls;
        if (!execution_result) {
            return {{}, execution_result.code, execution_result.message};
        }
        workflow::OrganizeOutcome outcome;
        if (execution_calls <=
            static_cast<int>(remaining_after_execution.size())) {
            outcome.remaining_moves =
                remaining_after_execution[execution_calls - 1];
        }
        outcome.message = outcome.remaining_moves == 0
            ? "Organize transaction committed."
            : "Move batch committed; more moves remain.";
        return {outcome, ErrorCode::Ok, outcome.message};
    }

    std::atomic<int> preview_calls{};
    int execution_calls{};
    std::atomic<std::uint64_t> last_scene_generation{};
    workflow::WorkflowCapability capability{
        workflow::WorkflowCapability::PreviewOnly};
    Result<void> execution_result{};
    std::vector<std::size_t> remaining_after_execution;
};

ui::UiContextSnapshot HouseContext(std::uint64_t generation = 1) {
    return {
        ui::UiContextKind::House,
        "House",
        generation,
        true,
        false,
        {"scene:House"}
    };
}

void FinishPreview(
    ui::HouseButtonController& controller,
    const FakeHouseButtonView& view) {
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::seconds(1);
    while (view.state == ui::OrganizeButtonState::Running &&
           std::chrono::steady_clock::now() < deadline) {
        controller.Poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    controller.Poll();
}

}  // namespace

void RunHouseButtonControllerTests() {
    using namespace std::chrono_literals;

    FakeHouseButtonView view;
    FakeWorkflow workflow;
    auto now = std::chrono::steady_clock::time_point{} + 1s;
    ui::HouseButtonController controller(
        view,
        workflow,
        [&now] {
            return now;
        });

    auto unsafe = HouseContext();
    unsafe.save_in_progress = true;
    AC_CHECK(!static_cast<bool>(controller.Attach(unsafe)));
    AC_CHECK(view.attach_calls == 0);

    AC_CHECK(static_cast<bool>(controller.Attach(HouseContext())));
    AC_CHECK(controller.IsAttached());
    AC_CHECK(view.attach_calls == 1);
    AC_CHECK(view.state == ui::OrganizeButtonState::Ready);
    AC_CHECK(workflow.preview_calls == 0);

    controller.SetSuppressed(true);
    AC_CHECK(controller.IsSuppressed());
    AC_CHECK(controller.IsAttached());
    AC_CHECK(view.detach_calls == 0);
    AC_CHECK(view.state == ui::OrganizeButtonState::Hidden);
    view.Click();
    AC_CHECK(workflow.preview_calls == 0);
    controller.SetSuppressed(false);
    AC_CHECK(!controller.IsSuppressed());
    AC_CHECK(controller.IsAttached());
    AC_CHECK(view.attach_calls == 1);
    AC_CHECK(view.detach_calls == 0);
    AC_CHECK(view.state == ui::OrganizeButtonState::Ready);

    AC_CHECK(static_cast<bool>(controller.Attach(HouseContext())));
    AC_CHECK(view.attach_calls == 1);

    view.Click();
    FinishPreview(controller, view);
    AC_CHECK(workflow.preview_calls == 1);
    AC_CHECK(workflow.last_scene_generation == 1);
    AC_CHECK(view.placeholder_calls == 1);
    AC_CHECK(view.state == ui::OrganizeButtonState::Completed);

    now += 499ms;
    controller.Poll();
    AC_CHECK(view.state == ui::OrganizeButtonState::Completed);
    view.Click();
    AC_CHECK(workflow.preview_calls == 1);

    now += 1ms;
    controller.Poll();
    AC_CHECK(view.state == ui::OrganizeButtonState::Ready);
    view.Click();
    FinishPreview(controller, view);
    AC_CHECK(workflow.preview_calls == 2);
    AC_CHECK(view.placeholder_calls == 2);

    for (int click = 0; click < 18; ++click) {
        now += 2s;
        controller.Poll();
        AC_CHECK(view.state == ui::OrganizeButtonState::Ready);
        view.Click();
        FinishPreview(controller, view);
    }
    AC_CHECK(workflow.preview_calls == 20);
    AC_CHECK(view.placeholder_calls == 20);

    controller.Detach();
    AC_CHECK(!controller.IsAttached());
    AC_CHECK(view.detach_calls == 1);
    AC_CHECK(view.state == ui::OrganizeButtonState::Hidden);

    controller.Detach();
    AC_CHECK(view.detach_calls == 1);

    controller.AbandonScene();
    AC_CHECK(view.abandon_calls == 1);

    FakeHouseButtonView replacement_view;
    FakeWorkflow replacement_workflow;
    ui::HouseButtonController replacement_controller(
        replacement_view,
        replacement_workflow);
    AC_CHECK(static_cast<bool>(replacement_controller.Attach(HouseContext())));
    AC_CHECK(static_cast<bool>(
        replacement_controller.Attach(HouseContext(2))));
    AC_CHECK(replacement_controller.IsAttachedToGeneration(2));
    AC_CHECK(replacement_view.abandon_calls == 1);
    AC_CHECK(replacement_view.attach_calls == 2);

    FakeHouseButtonView changed_view;
    FakeWorkflow changed_workflow;
    ui::HouseButtonController changed_controller(
        changed_view,
        changed_workflow,
        [&now] { return now; });
    auto first_house = HouseContext();
    first_house.scene_generation = 10;
    AC_CHECK(static_cast<bool>(changed_controller.Attach(first_house)));
    changed_view.Click();
    changed_controller.Detach();
    auto next_house = HouseContext();
    next_house.scene_generation = 11;
    AC_CHECK(static_cast<bool>(changed_controller.Attach(next_house)));
    for (int attempt = 0; attempt < 10000; ++attempt) {
        changed_controller.Poll();
        std::this_thread::yield();
    }
    AC_CHECK(changed_view.placeholder_calls == 0);
    AC_CHECK(changed_view.state == ui::OrganizeButtonState::Ready);

    FakeHouseButtonView move_view;
    FakeWorkflow move_workflow;
    move_workflow.capability =
        workflow::WorkflowCapability::MoveOnly;
    ui::HouseButtonController move_controller(
        move_view,
        move_workflow,
        [&now] { return now; });
    AC_CHECK(static_cast<bool>(move_controller.Attach(HouseContext())));
    move_view.Click();
    FinishPreview(move_controller, move_view);
    AC_CHECK(move_workflow.preview_calls == 1);
    AC_CHECK(move_workflow.execution_calls == 0);
    AC_CHECK(move_view.state == ui::OrganizeButtonState::Ready);
    move_view.Click();
    AC_CHECK(move_workflow.execution_calls == 0);
    move_controller.Poll();
    AC_CHECK(move_workflow.execution_calls == 1);
    AC_CHECK(move_workflow.preview_calls == 1);

    // A confirmed zero-move run must return to ready and accept another run.
    AC_CHECK(move_view.state == ui::OrganizeButtonState::Ready);
    move_view.Click();
    FinishPreview(move_controller, move_view);
    AC_CHECK(move_workflow.preview_calls == 2);
    move_view.Click();
    AC_CHECK(move_workflow.execution_calls == 1);
    move_controller.Poll();
    AC_CHECK(move_workflow.execution_calls == 2);
    AC_CHECK(move_view.state == ui::OrganizeButtonState::Ready);

    FakeHouseButtonView batched_view;
    FakeWorkflow batched_workflow;
    batched_workflow.capability =
        workflow::WorkflowCapability::MoveOnly;
    batched_workflow.remaining_after_execution = {35, 27, 19, 11, 3, 0};
    ui::HouseButtonController batched_controller(
        batched_view,
        batched_workflow,
        [&now] { return now; });
    AC_CHECK(static_cast<bool>(batched_controller.Attach(HouseContext())));
    batched_view.Click();
    FinishPreview(batched_controller, batched_view);
    batched_view.Click();
    AC_CHECK(batched_workflow.execution_calls == 0);
    batched_controller.Poll();
    AC_CHECK(batched_workflow.execution_calls == 1);
    AC_CHECK(batched_workflow.preview_calls == 1);
    batched_controller.Poll();
    for (int attempt = 0;
         attempt < 10000 && batched_workflow.preview_calls < 2;
         ++attempt) {
        std::this_thread::yield();
    }
    AC_CHECK(batched_workflow.preview_calls == 2);
    AC_CHECK(batched_workflow.execution_calls == 1);
    for (int attempt = 0;
         attempt < 10000 && batched_workflow.execution_calls < 6;
         ++attempt) {
        batched_controller.Poll();
        std::this_thread::yield();
    }
    AC_CHECK(batched_workflow.execution_calls == 6);
    AC_CHECK(batched_workflow.preview_calls == 6);
    AC_CHECK(batched_view.state == ui::OrganizeButtonState::Ready);

    FakeHouseButtonView cancelled_view;
    FakeWorkflow cancelled_workflow;
    cancelled_workflow.capability =
        workflow::WorkflowCapability::MoveOnly;
    cancelled_workflow.remaining_after_execution = {35, 27};
    ui::HouseButtonController cancelled_controller(
        cancelled_view,
        cancelled_workflow,
        [&now] { return now; });
    AC_CHECK(static_cast<bool>(cancelled_controller.Attach(HouseContext())));
    cancelled_view.Click();
    FinishPreview(cancelled_controller, cancelled_view);
    cancelled_view.Click();
    AC_CHECK(cancelled_workflow.execution_calls == 0);
    cancelled_controller.Poll();
    AC_CHECK(cancelled_workflow.execution_calls == 1);
    cancelled_controller.AbandonScene();
    for (int attempt = 0; attempt < 1000; ++attempt) {
        cancelled_controller.Poll();
        std::this_thread::yield();
    }
    AC_CHECK(cancelled_workflow.execution_calls == 1);
}

}  // namespace autocattery::tests
