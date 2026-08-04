#pragma once

#include <span>
#include <string_view>
#include <unordered_set>

namespace autocattery::ui {

struct UiNodeMatch {
    void* root{};
    void* node{};
};

template <typename ComponentMatcher, typename RootResolver,
          typename ChildFinder>
UiNodeMatch FindNodeInMatchingRoots(
    std::span<void* const> components,
    std::string_view component_type,
    std::string_view node_name,
    ComponentMatcher&& matches_component,
    RootResolver&& resolve_root,
    ChildFinder&& find_child) {
    std::unordered_set<void*> visited;
    visited.reserve(components.size());
    for (void* component : components) {
        if (!matches_component(component, component_type)) {
            continue;
        }
        void* root = resolve_root(component);
        if (root == nullptr || !visited.insert(root).second) {
            continue;
        }
        if (void* node = find_child(root, node_name); node != nullptr) {
            return {root, node};
        }
    }
    return {};
}

UiNodeMatch FindSceneUiNode(void* scene_manager, std::string_view node_name);

}  // namespace autocattery::ui
