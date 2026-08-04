#include "mew_ui_safe_node_lookup.hpp"

#include <string>

#include "mew_ui_scene_components.h"

namespace autocattery::ui {
namespace {

// Runtime mapping evidence for the supported build shows exactly one
// HouseTest component and one root. Restricting lookup to that observed UI
// owner avoids passing thousands of unrelated component roots into the game
// child lookup routine.
constexpr auto kHouseUiOwnerType = "HouseTest";

}  // namespace

UiNodeMatch FindSceneUiNode(
    void* scene_manager, std::string_view node_name) {
    auto* components = AcMewGetValidatedSceneComponents(scene_manager);
    if (components == nullptr || node_name.empty()) return {};
    const std::string owned_name(node_name);
    return FindNodeInMatchingRoots(
        std::span<void* const>(components->data, components->size),
        kHouseUiOwnerType,
        node_name,
        [](void* component, std::string_view component_type) {
            return AcMewComponentTypeEquals(
                       component, component_type.data()) != 0;
        },
        [](void* component) {
            return AcMewGetValidatedComponentRoot(component);
        },
        [&owned_name](void* root, std::string_view) {
            return MewUI_FindChildByName(root, owned_name.c_str());
        });
}

}  // namespace autocattery::ui
