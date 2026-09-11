#include "mew_ui_house_move_probe.h"
extern "C" {
#include "mew_ui_house_move_room_scan.h"
}

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

#include <windows.h>

#include "mew_ui_house_move_adapter.h"
#include "test_support.hpp"

namespace autocattery::tests {

void RunMewUiHouseMoveProbeTests() {
    constexpr std::size_t kPrepareRva = 0x96B470U;
    constexpr std::array<std::uint8_t, 36> kPrepareSignature{
        0x40, 0x56, 0x41, 0x56, 0x41, 0x57, 0x48, 0x83, 0xEC, 0x40,
        0x48, 0x8B, 0x41, 0x20, 0x48, 0x8B, 0xF1, 0x4C, 0x63, 0xFA,
        0x4D, 0x8B, 0xC7, 0x4D, 0x8B, 0xF7, 0x49, 0xC1, 0xE0, 0x04,
        0x42, 0x80, 0x7C, 0x00, 0x08, 0x00
    };
    std::vector<std::uint8_t> executable(kPrepareRva + 0x80U);
    // The previously accepted address must not resolve, even with matching bytes.
    std::copy(kPrepareSignature.begin(), kPrepareSignature.end(),
              executable.begin() + 0x963030U);
    AC_CHECK(AcMewSelectComponentBucketPrepareRva(
                 executable.data(), executable.size()) == 0U);
    std::copy(kPrepareSignature.begin(), kPrepareSignature.end(),
              executable.begin() + kPrepareRva);
    AC_CHECK(AcMewSelectComponentBucketPrepareRva(
                 executable.data(), executable.size()) == kPrepareRva);
    AC_CHECK(AcMewSelectComponentBucketPrepareRva(
                 executable.data(), kPrepareRva + 10) == 0U);
    executable[kPrepareRva] = 0U;
    AC_CHECK(AcMewSelectComponentBucketPrepareRva(
                 executable.data(), executable.size()) == 0U);

    const char fifth_room[] = "Floor2_Small";
    AC_CHECK((AcMewMoveRoomMask(reinterpret_cast<const std::uint8_t*>(fifth_room),
                              sizeof(fifth_room)) & (1U << 5)) != 0);

    constexpr std::size_t kBetaMoveRva = 0x2E88D0U;
    constexpr std::size_t kStableMoveRva = 0x2E7DB0U;
    constexpr std::array<std::uint8_t, 31> kMoveSignature{
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74,
        0x24, 0x10, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x48,
        0x8B, 0xF9, 0x48, 0x8B, 0xF2, 0x48, 0x8B, 0x49,
        0x70, 0x4C, 0x8B, 0xC2, 0x8B, 0x47, 0x6C
    };
    std::vector<std::uint8_t> move_image(kBetaMoveRva + 0x80U);
    std::copy_n(
        kMoveSignature.begin(),
        15,
        move_image.begin() + kStableMoveRva);
    AC_CHECK(AcMewSelectNativeHouseMoveRva(
                 move_image.data(), move_image.size()) == kStableMoveRva);
    std::fill(
        move_image.begin() + kStableMoveRva,
        move_image.begin() + kStableMoveRva + 15,
        std::uint8_t{0});
    std::copy(
        kMoveSignature.begin(),
        kMoveSignature.end(),
        move_image.begin() + kBetaMoveRva);
    AC_CHECK(AcMewSelectNativeHouseMoveRva(
                 move_image.data(), move_image.size()) == kBetaMoveRva);
    move_image[kBetaMoveRva] = 0U;
    AC_CHECK(AcMewSelectNativeHouseMoveRva(
                 move_image.data(), move_image.size()) == 0U);

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
