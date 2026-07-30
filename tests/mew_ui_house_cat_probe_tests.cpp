#include "../src/ui/mew_ui_house_cat_probe.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#include "mew_ui_api.h"
#include "test_support.hpp"

namespace autocattery::tests {
namespace {

MewNarrowString* __cdecl HouseCatType(
    const void*,
    MewNarrowString* result) {
    constexpr char kType[] = "HouseCat";
    std::memset(result, 0, sizeof(*result));
    std::memcpy(result->storage.inline_buf, kType, sizeof(kType) - 1);
    result->size = sizeof(kType) - 1;
    result->capacity = 15;
    return result;
}

MewNarrowString* __cdecl OtherType(
    const void*,
    MewNarrowString* result) {
    constexpr char kType[] = "Other";
    std::memset(result, 0, sizeof(*result));
    std::memcpy(result->storage.inline_buf, kType, sizeof(kType) - 1);
    result->size = sizeof(kType) - 1;
    result->capacity = 15;
    return result;
}

}  // namespace

void RunMewUiHouseCatProbeTests() {
    constexpr std::size_t kCatCount = 79;
    constexpr std::size_t kSceneComponentCount = 4'200;
    constexpr std::size_t kComponentSize = 0x800;
    constexpr std::size_t kIdentityOffset = 0x80;
    const MewComponentVTable vtable{
        &HouseCatType,
        nullptr,
        nullptr
    };
    const MewComponentVTable other_vtable{
        &OtherType,
        nullptr,
        nullptr
    };
    std::vector<std::array<std::uint8_t, kComponentSize>> storage(kCatCount);
    std::vector<void*> components;
    std::vector<void*> all_components;
    std::vector<std::int64_t> cat_ids;
    const MewComponentVTable* vtable_pointer = &vtable;
    const MewComponentVTable* other_vtable_pointer = &other_vtable;
    components.reserve(kCatCount);
    all_components.reserve(kSceneComponentCount);
    cat_ids.reserve(kCatCount);
    for (std::size_t index = 0; index < kCatCount; ++index) {
        auto& bytes = storage[index];
        bytes.fill(0);
        const auto id = static_cast<std::int64_t>(10'000 + index);
        std::memcpy(
            bytes.data(),
            &vtable_pointer,
            sizeof(vtable_pointer));
        void* root = bytes.data() + 0x100;
        std::memcpy(bytes.data() + 0x38, &root, sizeof(root));
        std::memcpy(
            bytes.data() + kIdentityOffset,
            &id,
            sizeof(id));
        components.push_back(bytes.data());
        all_components.push_back(bytes.data());
        cat_ids.push_back(id);
    }
    std::vector<std::array<std::uint8_t, sizeof(void*)>> filler_storage(
        kSceneComponentCount - kCatCount);
    for (auto& bytes : filler_storage) {
        bytes.fill(0);
        std::memcpy(
            bytes.data(),
            &other_vtable_pointer,
            sizeof(other_vtable_pointer));
        all_components.push_back(bytes.data());
    }

    MewPodVectorPtr component_list{
        static_cast<std::uint32_t>(all_components.size()),
        static_cast<std::uint32_t>(all_components.size()),
        all_components.data()
    };
    std::array<
        std::uint8_t,
        MEW_OFF_SCENE_COMPONENT_LISTS + sizeof(void*)> scene_manager{};
    auto* component_list_pointer = &component_list;
    std::memcpy(
        scene_manager.data() + MEW_OFF_SCENE_COMPONENT_LISTS,
        &component_list_pointer,
        sizeof(component_list_pointer));

    std::vector<AcMewHouseCatMatch> matches(kCatCount);
    MEMORY_BASIC_INFORMATION memory{};
    AC_CHECK(VirtualQuery(
        components.front(),
        &memory,
        sizeof(memory)) == sizeof(memory));
    AC_CHECK(memory.State == MEM_COMMIT);
    AC_CHECK(
        reinterpret_cast<std::uintptr_t>(memory.BaseAddress) +
            memory.RegionSize -
            reinterpret_cast<std::uintptr_t>(components.front()) >=
        kComponentSize);
    std::int64_t stored_identity{};
    std::memcpy(
        &stored_identity,
        static_cast<std::uint8_t*>(components.front()) + kIdentityOffset,
        sizeof(stored_identity));
    AC_CHECK(stored_identity == cat_ids.front());
    const auto result = AcMewProbeHouseCatIdentity(
        scene_manager.data(),
        cat_ids.data(),
        cat_ids.size(),
        matches.data(),
        matches.size());
    AC_CHECK(result.requested_cat_count == kCatCount);
    AC_CHECK(result.house_cat_count == kCatCount);
    AC_CHECK(result.match_count == kCatCount);
    AC_CHECK(result.stable_bijection != 0);
    AC_CHECK(result.consistent_mapping != 0);
    for (std::size_t index = 0; index < kCatCount; ++index) {
        AC_CHECK(matches[index].cat_id == cat_ids[index]);
        AC_CHECK(matches[index].component == components[index]);
        AC_CHECK(matches[index].root_node != nullptr);
    }

    SYSTEM_INFO system_info{};
    GetSystemInfo(&system_info);
    const auto page_size =
        static_cast<std::size_t>(system_info.dwPageSize);
    auto* boundary_region = static_cast<std::uint8_t*>(VirtualAlloc(
        nullptr,
        page_size * 2U,
        MEM_RESERVE | MEM_COMMIT,
        PAGE_READWRITE));
    AC_CHECK(boundary_region != nullptr);
    if (boundary_region != nullptr) {
        constexpr std::size_t kReadableTail = 0x100;
        auto* boundary_component =
            boundary_region + page_size - kReadableTail;
        std::memcpy(
            boundary_component,
            components.back(),
            kReadableTail);
        void* boundary_root = boundary_component + 0x90;
        std::memcpy(
            boundary_component + 0x38,
            &boundary_root,
            sizeof(boundary_root));
        components.back() = boundary_component;
        all_components[kCatCount - 1U] = boundary_component;

        DWORD previous_protection{};
        const auto protected_boundary = VirtualProtect(
            boundary_region + page_size,
            page_size,
            PAGE_NOACCESS,
            &previous_protection) != 0;
        AC_CHECK(protected_boundary);
        if (protected_boundary) {
            const auto boundary_result = AcMewProbeHouseCatIdentity(
                scene_manager.data(),
                cat_ids.data(),
                cat_ids.size(),
                matches.data(),
                matches.size());
            AC_CHECK(boundary_result.house_cat_count == kCatCount);
            AC_CHECK(boundary_result.match_count == kCatCount);
            AC_CHECK(boundary_result.first_identity_offset == kIdentityOffset);
            AC_CHECK(boundary_result.first_identity_width == 8U);
            AC_CHECK(boundary_result.stable_bijection != 0U);
            AC_CHECK(matches.back().component == boundary_component);
            AC_CHECK(matches.back().root_node == boundary_root);
        }
    }

    component_list.capacity = component_list.size - 1U;
    const auto invalid = AcMewProbeHouseCatIdentity(
        scene_manager.data(),
        cat_ids.data(),
        cat_ids.size(),
        matches.data(),
        matches.size());
    AC_CHECK(invalid.house_cat_count == 0U);
    AC_CHECK(invalid.match_count == 0U);
    AC_CHECK(invalid.stable_bijection == 0U);
    if (boundary_region != nullptr) {
        VirtualFree(boundary_region, 0U, MEM_RELEASE);
    }
}

}  // namespace autocattery::tests
