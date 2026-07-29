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

}  // namespace

void RunMewUiHouseCatProbeTests() {
    constexpr std::size_t kCatCount = 74;
    constexpr std::size_t kComponentSize = 0x800;
    constexpr std::size_t kIdentityOffset = 0x80;
    const MewComponentVTable vtable{
        &HouseCatType,
        nullptr,
        nullptr
    };
    std::vector<std::array<std::uint8_t, kComponentSize>> storage(kCatCount);
    std::vector<void*> components;
    std::vector<std::int64_t> cat_ids;
    const MewComponentVTable* vtable_pointer = &vtable;
    components.reserve(kCatCount);
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
        cat_ids.push_back(id);
    }

    MewPodVectorPtr component_list{
        static_cast<std::uint32_t>(components.size()),
        static_cast<std::uint32_t>(components.size()),
        components.data()
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
}

}  // namespace autocattery::tests
