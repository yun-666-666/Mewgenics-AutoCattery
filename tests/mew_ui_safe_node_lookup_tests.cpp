#include <array>
#include <cstddef>
#include <string_view>
#include <unordered_map>

#include "mew_ui_safe_node_lookup.hpp"
#include "mew_ui_scene_components.h"
#include "test_support.hpp"

// Mirror the native RTTI names without calling any game code.
namespace glaiel::swf {
class MovieClip {
public:
    virtual int Type() { return 2; }
    virtual void Slot1() {}
    virtual void Slot2() {}
    virtual void Prepare() {}
};
class DisplayObjectContainer : public MovieClip {};
class DisplayObjectContainer_DynamicBatched : public MovieClip {};
}  // namespace glaiel::swf

namespace autocattery::tests {
namespace {

void UiChildCollectionVirtualCallProbe() {}
class UnrelatedSceneObject {
public:
    virtual ~UnrelatedSceneObject() = default;
    virtual void Slot1() {}
    virtual void Slot2() {}
    virtual void Slot3() {}
};

}  // namespace

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
    std::size_t root_calls{};

    const auto match = ui::FindNodeInUniqueRoots(
        components,
        "panel_background",
        [&roots, &root_calls](void* component) {
            ++root_calls;
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
    AC_CHECK(root_calls == components.size());

    child_calls = 0;
    root_calls = 0;
    const auto missing = ui::FindNodeInUniqueRoots(
        components,
        "missing",
        [&roots, &root_calls](void* component) {
            ++root_calls;
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
    AC_CHECK(root_calls == components.size());

    alignas(void*) std::array<std::byte, 0x40> component{};
    alignas(void*) std::array<std::byte, 0x88> root{};
    alignas(void*) std::array<std::byte, sizeof(void*)> child_collection{};
    std::array<void*, 4> vtable{};
    *reinterpret_cast<void**>(component.data() + 0x38) = root.data();
    *reinterpret_cast<void**>(root.data() + 0x80) = child_collection.data();
    *reinterpret_cast<void***>(child_collection.data()) = vtable.data();
    vtable[0] = reinterpret_cast<void*>(&UiChildCollectionVirtualCallProbe);
    vtable[3] = reinterpret_cast<void*>(&UiChildCollectionVirtualCallProbe);
    // Readable objects with executable virtual slots are not proof of UI type.
    AC_CHECK(AcMewGetValidatedComponentRoot(component.data()) == nullptr);

    vtable[3] = reinterpret_cast<void*>(1);
    AC_CHECK(AcMewGetValidatedComponentRoot(component.data()) == nullptr);

    glaiel::swf::MovieClip clip;
    glaiel::swf::DisplayObjectContainer container;
    glaiel::swf::DisplayObjectContainer_DynamicBatched batched;
    UnrelatedSceneObject unrelated;
    *reinterpret_cast<void**>(root.data() + 0x80) = &unrelated;
    AC_CHECK(AcMewGetValidatedComponentRoot(component.data()) == nullptr);
    for (void* display : {static_cast<void*>(&clip),
                          static_cast<void*>(&container),
                          static_cast<void*>(&batched)}) {
        *reinterpret_cast<void**>(root.data() + 0x80) = display;
        AC_CHECK(AcMewGetValidatedComponentRoot(component.data()) == root.data());
    }
    *reinterpret_cast<void**>(root.data() + 0x80) = nullptr;
    AC_CHECK(AcMewGetValidatedComponentRoot(component.data()) == nullptr);
}

}  // namespace autocattery::tests
