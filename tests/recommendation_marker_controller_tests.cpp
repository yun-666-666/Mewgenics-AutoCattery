#include "auto_cattery/ui/recommendation_marker_controller.hpp"

#include <chrono>
#include <string>
#include <utility>
#include <vector>

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

class FakeRecommendationMarkerView final
    : public ui::RecommendationMarkerView {
public:
    Result<void> Attach(
        const ui::UiContextSnapshot&,
        ClickHandler handler) override {
        ++attach_calls;
        attached = true;
        click_handler = std::move(handler);
        events.push_back("attach");
        return {};
    }

    void Detach() noexcept override {
        ++detach_calls;
        attached = false;
        click_handler = {};
        events.push_back("detach");
    }

    void SetMarkerVisible(bool visible) override {
        marker_visible = visible;
        events.push_back(visible ? "marker-on" : "marker-off");
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
    bool marker_visible{};
    int attach_calls{};
    int detach_calls{};
    std::vector<std::string> events;
    ClickHandler click_handler;
};

ui::UiContextSnapshot RecommendationHouseContext(
    std::uint64_t generation = 1) {
    return {
        ui::UiContextKind::House,
        "House",
        generation,
        true,
        false,
        {"scene:House"}
    };
}

}  // namespace

void RunRecommendationMarkerControllerTests() {
    using namespace std::chrono_literals;

    FakeRecommendationMarkerView view;
    auto now = std::chrono::steady_clock::time_point{} + 1s;
    ui::RecommendationMarkerController controller(
        view,
        [&now] {
            return now;
        });

    AC_CHECK(controller.ShouldShow());
    AC_CHECK(
        static_cast<bool>(
            controller.Attach(RecommendationHouseContext())));
    AC_CHECK(controller.IsAttached());
    AC_CHECK(view.attach_calls == 1);

    AC_CHECK(
        static_cast<bool>(
            controller.Attach(RecommendationHouseContext())));
    AC_CHECK(view.attach_calls == 1);

    for (int click = 0; click < 50; ++click) {
        view.Click();
        now += 250ms;
    }
    AC_CHECK(!controller.MarkerVisible());
    AC_CHECK(!view.marker_visible);

    view.Click();
    AC_CHECK(controller.MarkerVisible());
    controller.ObserveRuntime(false, false, false);
    AC_CHECK(!controller.IsAttached());
    AC_CHECK(!controller.MarkerVisible());
    AC_CHECK(view.events.at(view.events.size() - 2) == "marker-off");
    AC_CHECK(view.events.back() == "detach");

    controller.ObserveRuntime(true, false, false);
    AC_CHECK(controller.ShouldShow());
    AC_CHECK(
        static_cast<bool>(
            controller.Attach(RecommendationHouseContext(2))));
    AC_CHECK(view.attach_calls == 2);

    controller.ObserveRuntime(false, false, true);
    AC_CHECK(!controller.ShouldShow());
    AC_CHECK(!controller.IsAttached());

    controller.ObserveRuntime(true, false, false);
    AC_CHECK(!controller.ShouldShow());
    AC_CHECK(
        !static_cast<bool>(
            controller.Attach(RecommendationHouseContext(3))));

    controller.ObserveRuntime(false, true, false);
    AC_CHECK(!controller.ShouldShow());
    controller.ObserveRuntime(true, false, false);
    AC_CHECK(controller.ShouldShow());
    AC_CHECK(
        static_cast<bool>(
            controller.Attach(RecommendationHouseContext(4))));
    AC_CHECK(view.attach_calls == 3);

    auto unsafe = RecommendationHouseContext(5);
    unsafe.save_in_progress = true;
    controller.Detach();
    AC_CHECK(!static_cast<bool>(controller.Attach(unsafe)));
}

}  // namespace autocattery::tests
