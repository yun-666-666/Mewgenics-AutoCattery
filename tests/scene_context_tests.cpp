#include "auto_cattery/ui/scene_context.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

ui::SceneObservation Ready(
    ui::UiContextKind kind,
    std::uintptr_t instance,
    const char* name) {
    return {
        kind,
        name,
        instance,
        true,
        true,
        false,
        {"scene:" + std::string(name)}
    };
}

void ObserveFrames(
    ui::SceneContextService& service,
    const ui::SceneObservation& observation,
    int count) {
    for (int frame = 0; frame < count; ++frame) {
        AC_CHECK(static_cast<bool>(service.Observe(observation)));
    }
}

}  // namespace

void RunSceneContextTests() {
    const auto signature_path =
        std::filesystem::temp_directory_path() /
        "auto_cattery_phase02_scene_signatures.json";
    {
        std::ofstream output(signature_path, std::ios::trunc);
        output << R"({
            "schema_version": 1,
            "house": {
                "scene_names": ["House"],
                "required_nodes_any": ["house_button_anchor"],
                "required_nodes_all": [],
                "forbidden_nodes": []
            },
            "embark_selection": {
                "scene_names": [],
                "required_nodes_any": [],
                "required_nodes_all": [],
                "forbidden_nodes": []
            }
        })";
    }
    const auto signatures = ui::LoadSceneSignatures(signature_path);
    AC_CHECK(static_cast<bool>(signatures));
    AC_CHECK(signatures.value.house.scene_names.size() == 1);
    AC_CHECK(
        signatures.value.house.required_nodes_any.front() ==
        "house_button_anchor");

    ui::SceneContextService service(3, 10);
    AC_CHECK(static_cast<bool>(service.Start()));

    std::vector<ui::UiContextSnapshot> events;
    service.Subscribe([&events](const auto& snapshot) {
        events.push_back(snapshot);
    });
    service.Subscribe([](const auto&) {
        throw std::runtime_error("subscriber failure");
    });

    const auto house = Ready(ui::UiContextKind::House, 0x1000, "House");
    ObserveFrames(service, house, 2);
    AC_CHECK(service.Current().kind == ui::UiContextKind::Unknown);
    ObserveFrames(service, house, 1);
    AC_CHECK(service.Current().kind == ui::UiContextKind::House);
    AC_CHECK(service.Current().scene_generation == 1);
    AC_CHECK(events.size() == 1);

    ObserveFrames(service, house, 5);
    AC_CHECK(events.size() == 1);

    auto animated_house = house;
    animated_house.matched_signatures.push_back("dynamic-cat-layout-changed");
    ObserveFrames(service, animated_house, 3);
    AC_CHECK(service.Current().scene_generation == 1);
    AC_CHECK(events.size() == 1);

    ui::SceneObservation missing{};
    ObserveFrames(service, missing, 2);
    AC_CHECK(service.Current().kind == ui::UiContextKind::House);
    ObserveFrames(service, house, 1);
    AC_CHECK(service.Current().kind == ui::UiContextKind::House);

    ObserveFrames(service, missing, 10);
    AC_CHECK(
        service.Current().kind == ui::UiContextKind::UnsafeTransition);
    AC_CHECK(service.Current().scene_generation == 2);

    const auto embark = Ready(
        ui::UiContextKind::EmbarkSelection,
        0x2000,
        "EmbarkProbe");
    ObserveFrames(service, embark, 3);
    AC_CHECK(
        service.Current().kind == ui::UiContextKind::EmbarkSelection);
    AC_CHECK(service.Current().scene_generation == 3);

    ui::SceneObservation saving = embark;
    saving.save_in_progress = true;
    saving.input_enabled = false;
    AC_CHECK(static_cast<bool>(service.Observe(saving)));
    AC_CHECK(
        service.Current().kind == ui::UiContextKind::UnsafeTransition);
    AC_CHECK(service.Current().save_in_progress);
    AC_CHECK(service.Current().scene_generation == 4);

    const auto rapid_house =
        Ready(ui::UiContextKind::House, 0x3000, "House");
    ObserveFrames(service, rapid_house, 1);
    ObserveFrames(service, embark, 1);
    ObserveFrames(service, rapid_house, 1);
    AC_CHECK(
        service.Current().kind == ui::UiContextKind::UnsafeTransition);

    service.Stop();
    AC_CHECK(
        !static_cast<bool>(service.Observe(rapid_house)));
}

}  // namespace autocattery::tests
