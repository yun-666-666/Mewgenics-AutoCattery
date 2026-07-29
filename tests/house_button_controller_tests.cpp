#include "auto_cattery/ui/house_button_controller.hpp"

#include <chrono>
#include <utility>

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

    int preview_calls{};
    std::uint64_t last_scene_generation{};
};

ui::UiContextSnapshot HouseContext() {
    return {
        ui::UiContextKind::House,
        "House",
        1,
        true,
        false,
        {"scene:House"}
    };
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

    AC_CHECK(static_cast<bool>(controller.Attach(HouseContext())));
    AC_CHECK(view.attach_calls == 1);

    view.Click();
    AC_CHECK(workflow.preview_calls == 1);
    AC_CHECK(workflow.last_scene_generation == 1);
    AC_CHECK(view.placeholder_calls == 1);
    AC_CHECK(view.state == ui::OrganizeButtonState::Completed);

    now += 499ms;
    view.Click();
    AC_CHECK(workflow.preview_calls == 1);

    now += 1ms;
    view.Click();
    AC_CHECK(workflow.preview_calls == 2);
    AC_CHECK(view.placeholder_calls == 2);

    for (int click = 0; click < 18; ++click) {
        now += 500ms;
        view.Click();
    }
    AC_CHECK(workflow.preview_calls == 20);
    AC_CHECK(view.placeholder_calls == 20);

    controller.Detach();
    AC_CHECK(!controller.IsAttached());
    AC_CHECK(view.detach_calls == 1);
    AC_CHECK(view.state == ui::OrganizeButtonState::Hidden);

    controller.Detach();
    AC_CHECK(view.detach_calls == 1);
}

}  // namespace autocattery::tests
