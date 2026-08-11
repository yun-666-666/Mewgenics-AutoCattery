#include "mew_ui_house_move_probe.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

#include <windows.h>

#include "test_support.hpp"

namespace autocattery::tests {

void RunMewUiHouseMoveProbeTests() {
    std::array<std::uint8_t, AC_MEW_MOVE_PROBE_COMPONENT_BYTES> first{};
    std::array<std::uint8_t, AC_MEW_MOVE_PROBE_COMPONENT_BYTES> second{};
    std::array<std::uint8_t, AC_MEW_MOVE_PROBE_ROOT_BYTES> first_root{};
    std::array<std::uint8_t, AC_MEW_MOVE_PROBE_ROOT_BYTES> second_root{};
    constexpr std::size_t kRoomOffset = 0x300U;
    constexpr std::size_t kCoordinateOffset = 0x400U;
    constexpr char kOriginalRoom[] = "Floor1_Large";
    constexpr char kTargetRoom[] = "Attic";
    constexpr std::size_t kRoomPointerOffset = 0x500U;
    const char* original_room_pointer = kOriginalRoom;
    const char* target_room_pointer = kTargetRoom;
    const std::array<double, 3> original_coordinates{1.0, 2.0, 3.0};
    const std::array<double, 3> target_coordinates{4.0, 5.0, 6.0};
    std::memcpy(
        first.data() + kRoomOffset,
        kOriginalRoom,
        sizeof(kOriginalRoom));
    std::memcpy(
        first.data() + kCoordinateOffset,
        original_coordinates.data(),
        sizeof(original_coordinates));
    std::memcpy(
        first.data() + kRoomPointerOffset,
        &original_room_pointer,
        sizeof(original_room_pointer));

    const std::array<AcMewHouseCatMatch, 2> matches{{
        {101, first.data(), first_root.data()},
        {202, second.data(), second_root.data()}
    }};
    std::array<AcMewHouseMoveSample, 2> before{};
    std::array<AcMewHouseMoveSample, 2> after{};
    AC_CHECK(
        AcMewCaptureHouseMoveSamples(
            matches.data(), matches.size(), before.data(), before.size()) ==
        matches.size());

    std::memset(
        first.data() + kRoomOffset,
        0,
        sizeof(kOriginalRoom));
    std::memcpy(
        first.data() + kRoomOffset,
        kTargetRoom,
        sizeof(kTargetRoom));
    std::memcpy(
        first.data() + kCoordinateOffset,
        target_coordinates.data(),
        sizeof(target_coordinates));
    std::memcpy(
        first.data() + kRoomPointerOffset,
        &target_room_pointer,
        sizeof(target_room_pointer));
    AC_CHECK(
        AcMewCaptureHouseMoveSamples(
            matches.data(), matches.size(), after.data(), after.size()) ==
        matches.size());

    std::array<AcMewHouseMoveDiff, 2> differences{};
    const auto difference_count = AcMewCompareHouseMoveSamples(
        before.data(),
        before.size(),
        after.data(),
        after.size(),
        differences.data(),
        differences.size());
    AC_CHECK(difference_count == 1U);
    AC_CHECK(differences[0].cat_id == 101);
    AC_CHECK(differences[0].component_changed_bytes > 0U);
    AC_CHECK(differences[0].root_changed_bytes == 0U);
    AC_CHECK((differences[0].component_rooms_before & 1U) != 0U);
    AC_CHECK((differences[0].component_rooms_after & (1U << 3U)) != 0U);
    const auto pointer_before = std::find_if(
        differences[0].room_references_before,
        differences[0].room_references_before +
            differences[0].room_references_before_count,
        [](const AcMewMoveRoomReference& reference) {
            return reference.offset == kRoomPointerOffset &&
                   reference.room_index == 0U &&
                   reference.pointer_reference != 0U;
        });
    AC_CHECK(pointer_before !=
        differences[0].room_references_before +
            differences[0].room_references_before_count);
    const auto pointer_after = std::find_if(
        differences[0].room_references_after,
        differences[0].room_references_after +
            differences[0].room_references_after_count,
        [](const AcMewMoveRoomReference& reference) {
            return reference.offset == kRoomPointerOffset &&
                   reference.room_index == 3U &&
                   reference.pointer_reference != 0U;
        });
    AC_CHECK(pointer_after !=
        differences[0].room_references_after +
            differences[0].room_references_after_count);
    const auto coordinate = std::find_if(
        differences[0].double_candidates,
        differences[0].double_candidates +
            differences[0].double_candidate_count,
        [](const AcMewMoveDoubleCandidate& candidate) {
            return candidate.region == AC_MEW_MOVE_PROBE_COMPONENT &&
                   candidate.offset == kCoordinateOffset;
        });
    AC_CHECK(
        coordinate != differences[0].double_candidates +
            differences[0].double_candidate_count);
    if (coordinate != differences[0].double_candidates +
            differences[0].double_candidate_count) {
        AC_CHECK(coordinate->before[0] == 1.0);
        AC_CHECK(coordinate->after[2] == 6.0);
    }
    AC_CHECK(
        std::strcmp(AcMewMoveProbeRoomId(3U), "Attic") == 0);
    AC_CHECK(
        std::strcmp(AcMewMoveProbeRoomId(4U), "AdventureBox") == 0);
    AC_CHECK(
        std::strcmp(AcMewMoveProbeRoomId(5U), "Floor2_Small") == 0);
    AC_CHECK(AcMewMoveProbeRoomId(AC_MEW_MOVE_PROBE_ROOM_COUNT) == nullptr);

    SYSTEM_INFO system_info{};
    GetSystemInfo(&system_info);
    const auto page_size =
        static_cast<std::size_t>(system_info.dwPageSize);
    auto* pages = static_cast<std::uint8_t*>(VirtualAlloc(
        nullptr,
        page_size * 2U,
        MEM_RESERVE | MEM_COMMIT,
        PAGE_READWRITE));
    AC_CHECK(pages != nullptr);
    if (pages != nullptr) {
        DWORD previous{};
        AC_CHECK(VirtualProtect(
            pages + page_size,
            page_size,
            PAGE_NOACCESS,
            &previous) != 0);
        const AcMewHouseCatMatch boundary{
            303,
            pages + page_size - 0x100U,
            nullptr
        };
        AcMewHouseMoveSample sample{};
        AC_CHECK(AcMewCaptureHouseMoveSamples(
            &boundary, 1U, &sample, 1U) == 0U);

        const AcMewHouseCatMatch partial{
            404,
            pages + page_size - 0x300U,
            nullptr
        };
        AC_CHECK(AcMewCaptureHouseMoveSamples(
            &partial, 1U, &sample, 1U) == 1U);
        AC_CHECK(sample.component_size == 0x300U);
        VirtualFree(pages, 0U, MEM_RELEASE);
    }
}

}  // namespace autocattery::tests
