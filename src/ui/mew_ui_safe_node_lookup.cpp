#include "mew_ui_safe_node_lookup.hpp"

#include <string>

#include "mew_ui_scene_components.h"

namespace autocattery::ui {

UiNodeMatch FindSceneUiNode(
    void* scene_manager, std::string_view node_name) {
    auto* components = AcMewGetValidatedSceneComponents(scene_manager);
    if (components == nullptr || node_name.empty()) return {};
    const std::string owned_name(node_name);
    return FindNodeInUniqueRoots(
        std::span<void* const>(components->data, components->size),
        node_name,
        [](void* component) {
            return AcMewGetValidatedComponentRoot(component);
        },
        [&owned_name](void* root, std::string_view) {
            return MewUI_FindChildByName(root, owned_name.c_str());
        });
}

}  // namespace autocattery::ui
