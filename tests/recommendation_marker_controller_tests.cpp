#include "auto_cattery/ui/recommendation_marker_controller.hpp"

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
        const ui::UiContextSnapshot& context,
        ClickHandler handler) override {
        ++attach_calls;
        attached = true;
        generation = context.scene_generation;
        click_handler = std::move(handler);
        events.emplace_back("attach");
        return {};
    }

    Result<ui::RecommendationVisual> ShowDemoTextMarker(
        std::uint64_t scene_generation) override {
        ++show_calls;
        events.emplace_back("show");
        if (!attached || fail_show || scene_generation != generation) {
            return {
                {},
                ErrorCode::SceneUnavailable,
                "fake marker unavailable"
            };
        }
        marker_visible = true;
        return {
            {
                &safe_target,
                nullptr,
                nullptr,
                &safe_target,
                scene_generation
            },
            ErrorCode::Ok,
            {}
        };
    }

    void ClearAllVisuals() noexcept override {
        ++clear_calls;
        marker_visible = false;
        events.emplace_back("clear");
    }

    void Detach() noexcept override {
        ++detach_calls;
        attached = false;
        click_handler = {};
        events.emplace_back("detach");
    }

    void SetButtonState(
        ui::RecommendationButtonState new_state) override {
        state = new_state;
    }

    [[nodiscard]] bool IsAttached() const noexcept override {
        return attached;
    }

    [[nodiscard]] void* SafeTestTarget() const noexcept override {
        return const_cast<int*>(&safe_target);
    }

    void Click() {
        if (click_handler) {
            click_handler();
        }
    }

    int safe_target{};
    bool attached{};
    bool marker_visible{};
    bool fail_show{};
    int attach_calls{};
    int show_calls{};
    int clear_calls{};
    int detach_calls{};
    std::uint64_t generation{};
    ui::RecommendationButtonState state{
        ui::RecommendationButtonState::Hidden};
    ClickHandler click_handler;
    std::vector<std::string> events;
};

ui::UiContextSnapshot HouseDepartureContext(
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
    FakeRecommendationMarkerView view;
    ui::RecommendationMarkerController controller(view);

    auto wrong_scene = HouseDepartureContext();
    wrong_scene.kind = ui::UiContextKind::EmbarkSelection;
    AC_CHECK(!static_cast<bool>(controller.AttachButton(wrong_scene)));
    AC_CHECK(view.attach_calls == 0);

    auto unsafe = HouseDepartureContext();
    unsafe.save_in_progress = true;
    AC_CHECK(!static_cast<bool>(controller.AttachButton(unsafe)));
    AC_CHECK(view.attach_calls == 0);

    AC_CHECK(
        static_cast<bool>(
            controller.AttachButton(HouseDepartureContext())));
    AC_CHECK(controller.IsAttached());
    AC_CHECK(controller.SceneGeneration() == 1);
    AC_CHECK(view.state == ui::RecommendationButtonState::Ready);

    AC_CHECK(
        static_cast<bool>(
            controller.AttachButton(HouseDepartureContext())));
    AC_CHECK(view.attach_calls == 1);

    for (int click = 0; click < 50; ++click) {
        view.Click();
        AC_CHECK(controller.HasMarkers() == (click % 2 == 0));
        AC_CHECK(view.marker_visible == controller.HasMarkers());
    }
    AC_CHECK(view.show_calls == 25);
    AC_CHECK(view.clear_calls == 25);
    AC_CHECK(view.state == ui::RecommendationButtonState::Ready);

    view.Click();
    AC_CHECK(controller.HasMarkers());
    const auto event_count = view.events.size();
    AC_CHECK(static_cast<bool>(
        controller.AttachButton(HouseDepartureContext(2))));
    AC_CHECK(view.attach_calls == 2);
    AC_CHECK(view.detach_calls == 1);
    AC_CHECK(!controller.HasMarkers());
    AC_CHECK(controller.SceneGeneration() == 2);
    AC_CHECK(view.events[event_count] == "clear");
    AC_CHECK(view.events[event_count + 1] == "detach");

    view.fail_show = true;
    view.Click();
    AC_CHECK(!controller.HasMarkers());
    AC_CHECK(view.state == ui::RecommendationButtonState::Ready);

    controller.Detach();
    AC_CHECK(!controller.IsAttached());
    AC_CHECK(view.detach_calls == 2);
    AC_CHECK(view.state == ui::RecommendationButtonState::Hidden);

    controller.Detach();
    AC_CHECK(view.detach_calls == 2);
}

}  // namespace autocattery::tests
