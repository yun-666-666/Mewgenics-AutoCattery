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
        ClickHandler handler,
        ItemClickHandler item_handler) override {
        ++attach_calls;
        attached = true;
        click_handler = std::move(handler);
        item_click_handler = std::move(item_handler);
        events.push_back("attach");
        return {};
    }

    void Detach() noexcept override {
        ++detach_calls;
        attached = false;
        click_handler = {};
        item_click_handler = {};
        events.push_back("detach");
    }

    void SetStatus(ui::RecommendationUiStatus status) override {
        marker_visible =
            status == ui::RecommendationUiStatus::Marked;
        probe_required =
            status == ui::RecommendationUiStatus::ProbeRequired;
        events.push_back(
            probe_required
                ? "probe-required"
                : (marker_visible ? "marked" : "marker-off"));
    }

    Result<void> ShowItems(
        const std::vector<std::string>& values) override {
        labels = values;
        events.push_back("summary-on");
        return {};
    }

    void ClearSummary() noexcept override {
        labels.clear();
        events.push_back("summary-off");
    }

    void Poll() override {
        ++poll_calls;
    }

    [[nodiscard]] bool IsAttached() const noexcept override {
        return attached;
    }

    void Click() {
        if (click_handler) {
            click_handler();
        }
    }

    void ClickItem(std::size_t index) {
        if (item_click_handler) {
            item_click_handler(index);
        }
    }

    bool attached{};
    bool marker_visible{};
    bool probe_required{};
    std::vector<std::string> labels;
    int attach_calls{};
    int detach_calls{};
    int poll_calls{};
    std::vector<std::string> events;
    ClickHandler click_handler;
    ItemClickHandler item_click_handler;
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
    AC_CHECK(!controller.MarkerVisible());
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

    FakeRecommendationMarkerView probe_view;
    int requests{};
    std::uint64_t requested_generation{};
    ui::RecommendationMarkerController probe_controller(
        probe_view,
        [&now] {
            return now;
        });
    probe_controller.SetRequestHandler(
        [&](std::uint64_t generation) {
            ++requests;
            requested_generation = generation;
        });
    AC_CHECK(static_cast<bool>(
        probe_controller.Attach(RecommendationHouseContext(12))));
    AC_CHECK(requests == 0);
    probe_view.Click();
    AC_CHECK(requests == 1);
    AC_CHECK(requested_generation == 12);
    AC_CHECK(probe_view.probe_required);
    AC_CHECK(!probe_controller.MarkerVisible());
    probe_controller.CompleteProbe(11);
    AC_CHECK(probe_view.probe_required);
    probe_controller.CompleteProbe(12);
    AC_CHECK(probe_view.probe_required);
    now += 1999ms;
    probe_controller.Poll();
    AC_CHECK(probe_view.poll_calls == 1);
    AC_CHECK(probe_view.probe_required);
    probe_view.Click();
    AC_CHECK(requests == 1);
    now += 1ms;
    probe_controller.Poll();
    AC_CHECK(probe_view.poll_calls == 2);
    AC_CHECK(!probe_view.probe_required);
    probe_view.Click();
    AC_CHECK(requests == 2);

    AC_CHECK(!static_cast<bool>(
        probe_controller.ShowRecommendations(11, {"stale"})));
    AC_CHECK(probe_view.labels.empty());
    AC_CHECK(static_cast<bool>(
        probe_controller.ShowRecommendations(
            12,
            {"#1 Mew 42.0 ?", "#2 Purr 40.0 ?"})));
    AC_CHECK(probe_controller.MarkerVisible());
    AC_CHECK(probe_view.probe_required);
    AC_CHECK(!probe_view.marker_visible);
    AC_CHECK(probe_view.labels.size() == 2);
    now += 1999ms;
    probe_controller.Poll();
    AC_CHECK(probe_view.probe_required);
    now += 1ms;
    probe_controller.Poll();
    AC_CHECK(probe_view.marker_visible);
    AC_CHECK(probe_controller.MarkerVisible());

    std::size_t detail_index = 99;
    std::uint64_t detail_generation{};
    probe_controller.SetDetailsHandler(
        [&](std::uint64_t generation, std::size_t index) {
            detail_generation = generation;
            detail_index = index;
        });
    probe_view.ClickItem(1);
    AC_CHECK(detail_generation == 12);
    AC_CHECK(detail_index == 1);
    probe_view.ClickItem(2);
    AC_CHECK(detail_index == 1);

    probe_view.Click();
    AC_CHECK(!probe_controller.MarkerVisible());
    AC_CHECK(!probe_view.marker_visible);
    AC_CHECK(probe_view.labels.empty());
    AC_CHECK(requests == 2);
}

}  // namespace autocattery::tests
