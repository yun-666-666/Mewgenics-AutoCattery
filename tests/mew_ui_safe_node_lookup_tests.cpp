#include <array>
#include <string_view>
#include <unordered_map>

#include "mew_ui_safe_node_lookup.hpp"
#include "test_support.hpp"

namespace autocattery::tests {

void RunMewUiSafeNodeLookupTests() {
    void* component_a = reinterpret_cast<void*>(0x10);
    void* component_b = reinterpret_cast<void*>(0x20);
    void* component_c = reinterpret_cast<void*>(0x30);
    void* root_a = reinterpret_cast<void*>(0x100);
    void* root_b = reinterpret_cast<void*>(0x200);
    void* expected = reinterpret_cast<void*>(0x300);
    const std::array<void*, 5> components{
        nullptr, component_a, component_b, component_a, component_c};
    const std::unordered_map<void*, void*> roots{
        {component_a, root_a}, {component_b, root_a}, {component_c, root_b}};
    std::size_t child_calls{};

    const auto match = ui::FindNodeInUniqueRoots(
        components,
        "panel_background",
        [&roots](void* component) {
            const auto found = roots.find(component);
            return found == roots.end() ? nullptr : found->second;
        },
        [&](void* root, std::string_view name) -> void* {
            ++child_calls;
            AC_CHECK(name == "panel_background");
            return root == root_b ? expected : nullptr;
        });

    AC_CHECK(match.root == root_b);
    AC_CHECK(match.node == expected);
    AC_CHECK(child_calls == 2);

    child_calls = 0;
    const auto missing = ui::FindNodeInUniqueRoots(
        components,
        "missing",
        [&roots](void* component) {
            const auto found = roots.find(component);
            return found == roots.end() ? nullptr : found->second;
        },
        [&](void*, std::string_view) -> void* {
            ++child_calls;
            return nullptr;
        });
    AC_CHECK(missing.root == nullptr);
    AC_CHECK(missing.node == nullptr);
    AC_CHECK(child_calls == 2);
}

}  // namespace autocattery::tests
