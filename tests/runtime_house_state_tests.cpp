#include "runtime_house_state.hpp"
#include "runtime_house_move_gateway.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

snapshot::HouseSnapshot TwoRoomSnapshot() {
    snapshot::HouseSnapshot snapshot;
    snapshot.capabilities.read_room_assignments = true;
    snapshot.cats = {
        {.id = 1, .room_id = "Floor1_Large"},
        {.id = 2, .room_id = "Floor1_Large"},
        {.id = 3, .room_id = "Attic"},
        {.id = 4, .room_id = "Attic"}
    };
    snapshot.rooms = {
        {.id = "Floor1_Large", .residents = {1, 2}},
        {.id = "Attic", .residents = {3, 4}}
    };
    return snapshot;
}

snapshot::HouseSnapshot FiveRoomSnapshotWithEmptyFifthRoom() {
    snapshot::HouseSnapshot snapshot;
    snapshot.capabilities.read_room_assignments = true;
    snapshot.cats = {
        {.id = 1, .room_id = "Floor1_Large"},
        {.id = 2, .room_id = "Attic"},
        {.id = 3, .room_id = "Floor1_Small"},
        {.id = 4, .room_id = "Floor2_Large"}
    };
    snapshot.rooms = {
        {.id = "Floor1_Large", .residents = {1}},
        {.id = "Attic", .residents = {2}},
        {.id = "Floor1_Small", .residents = {3}},
        {.id = "Floor2_Large", .residents = {4}},
        {.id = "Floor2_Small"}
    };
    return snapshot;
}

ui::RuntimeHouseState SevenNativeRoomsWithEmptyFifthRoom() {
    return {
        .available_room_count = 5,
        .cats = {
            {1, 1001, 100},
            {2, 1002, 200},
            {3, 1003, 300},
            {4, 1004, 400}
        },
        .rooms = {
            {100, {"Floor1_Large"}},
            {200, {"Attic"}},
            {300, {"Floor1_Small"}},
            {400, {"Floor2_Large"}},
            {500, {"Floor2_Small"}},
            {600, {"AdventureBox"}},
            {700, {}}
        }
    };
}

ui::RuntimeHouseState RuntimeState(
    ui::RuntimePointer first_room_for_cat_one) {
    return {
        .available_room_count = 2,
        .cats = {
            {1, 1001, first_room_for_cat_one},
            {2, 1002, 100},
            {3, 1003, 200},
            {4, 1004, 200}
        },
        .rooms = {
            {100, {"Floor1_Large", "Attic"}},
            {200, {"Attic"}}
        }
    };
}

const snapshot::RoomSnapshot& Room(
    const snapshot::HouseSnapshot& snapshot,
    std::string_view id) {
    const auto room = std::ranges::find_if(
        snapshot.rooms,
        [id](const auto& candidate) { return candidate.id == id; });
    AC_CHECK(room != snapshot.rooms.end());
    return *room;
}

snapshot::detail::FurniturePlacement Furniture(
    std::int64_t key,
    std::string item,
    std::string room,
    std::int32_t x,
    std::int32_t y) {
    return {
        .instance_id = key,
        .item_id = std::move(item),
        .room_id = std::move(room),
        .position_x = x,
        .position_y = y,
        .scale_x = 1,
        .scale_y = 1};
}

ui::RuntimeFurniturePlacementState RuntimeFurniture(
    std::uint64_t key,
    std::string item,
    std::string room,
    std::int32_t x,
    std::int32_t y) {
    return {
        .stable_key = key,
        .item_id = std::move(item),
        .room_id = std::move(room),
        .position_x = x,
        .position_y = y,
        .scale_x = 1,
        .scale_y = 1};
}

}  // namespace

void RunRuntimeHouseStateTests() {
    room_planning::RoomPlan no_op;
    no_op.fully_satisfied = true;
    no_op.disposition = room_planning::PlanDisposition::Complete;
    AC_CHECK(ui::IsRuntimeMovePlanApproved(no_op));
    no_op.validation_errors.push_back("synthetic-error");
    AC_CHECK(!ui::IsRuntimeMovePlanApproved(no_op));
    AC_CHECK(ui::RuntimeHouseMoveBatchSize(0) == 0);
    AC_CHECK(ui::RuntimeHouseMoveBatchSize(8) == 8);
    AC_CHECK(ui::RuntimeHouseMoveBatchSize(43) == 8);

    const auto original = TwoRoomSnapshot();
    const auto unchanged = RuntimeState(100);
    AC_CHECK(ui::RuntimeHouseStateMatches(original, unchanged));

    const auto manually_moved = RuntimeState(200);
    AC_CHECK(!ui::RuntimeHouseStateMatches(original, manually_moved));
    auto overlaid = original;
    AC_CHECK(static_cast<bool>(
        ui::OverlayRuntimeHouseState(overlaid, manually_moved)));
    AC_CHECK(ui::RuntimeHouseStateMatches(overlaid, manually_moved));
    AC_CHECK(Room(overlaid, "Floor1_Large").residents.size() == 1);
    AC_CHECK(Room(overlaid, "Attic").residents.size() == 3);

    const auto fixed_mapping =
        ui::ResolveRuntimeRoomPointers(original, unchanged);
    AC_CHECK(static_cast<bool>(fixed_mapping));
    auto swapped_runtime = unchanged;
    swapped_runtime.cats[0].room = 200;
    swapped_runtime.cats[1].room = 200;
    swapped_runtime.cats[2].room = 100;
    swapped_runtime.cats[3].room = 100;
    auto stable_overlay = original;
    AC_CHECK(static_cast<bool>(ui::OverlayRuntimeHouseState(
        stable_overlay, swapped_runtime, fixed_mapping.value)));
    AC_CHECK(stable_overlay.cats[0].room_id == "Attic");
    AC_CHECK(stable_overlay.cats[2].room_id == "Floor1_Large");

    auto outside = unchanged;
    outside.cats.front().room = 0;
    auto outside_overlay = original;
    AC_CHECK(static_cast<bool>(
        ui::OverlayRuntimeHouseState(outside_overlay, outside)));
    AC_CHECK(!outside_overlay.cats.front().room_id.has_value());
    AC_CHECK(Room(outside_overlay, "Floor1_Large").residents.size() == 1);
    AC_CHECK(ui::RuntimeHouseStateMatches(outside_overlay, outside));

    auto changed_identity = unchanged;
    changed_identity.cats.back().cat_id = 99;
    AC_CHECK(!static_cast<bool>(
        ui::OverlayRuntimeHouseState(overlaid, changed_identity)));

    auto ambiguous = original;
    for (auto& cat : ambiguous.cats) {
        cat.room_id.reset();
    }
    for (auto& room : ambiguous.rooms) {
        room.residents.clear();
    }
    auto no_evidence = unchanged;
    no_evidence.rooms.clear();
    AC_CHECK(!static_cast<bool>(
        ui::ResolveRuntimeRoomPointers(ambiguous, no_evidence)));

    const auto five_rooms = FiveRoomSnapshotWithEmptyFifthRoom();
    const auto seven_native_rooms = SevenNativeRoomsWithEmptyFifthRoom();
    const auto five_room_mapping =
        ui::ResolveRuntimeRoomPointers(five_rooms, seven_native_rooms);
    AC_CHECK(static_cast<bool>(five_room_mapping));
    if (five_room_mapping) {
        AC_CHECK(five_room_mapping.value.size() == 5U);
        AC_CHECK(five_room_mapping.value.at("Floor2_Small") == 500U);
    }

    auto unknown_cat_room = seven_native_rooms;
    unknown_cat_room.cats.front().room = 600U;
    AC_CHECK(!static_cast<bool>(
        ui::ResolveRuntimeRoomPointers(five_rooms, unknown_cat_room)));

    std::vector<snapshot::detail::FurniturePlacement> furniture{
        Furniture(10, "base", "RoomA", 3, -11),
        Furniture(20, "stored", "", 0, 0)};
    ui::RuntimeFurnitureState live{
        .placements = {
            RuntimeFurniture(10, "base", "RoomA", 0, -11)}};
    AC_CHECK(static_cast<bool>(
        ui::OverlayRuntimeFurnitureState(furniture, live)));
    AC_CHECK(furniture[0].position_x == 0);
    live.placements[0].position_x = 2;
    live.placements[0].position_y = -9;
    AC_CHECK(static_cast<bool>(
        ui::OverlayRuntimeFurnitureState(furniture, live)));
    AC_CHECK(furniture[0].position_x == 2);
    AC_CHECK(furniture[0].position_y == -9);

    auto mismatched = live;
    mismatched.placements[0].item_id = "different";
    auto mismatch_source = furniture;
    AC_CHECK(!static_cast<bool>(
        ui::OverlayRuntimeFurnitureState(mismatch_source, mismatched)));

    auto duplicate = live;
    duplicate.placements.push_back(duplicate.placements.front());
    auto duplicate_source = furniture;
    AC_CHECK(!static_cast<bool>(
        ui::OverlayRuntimeFurnitureState(duplicate_source, duplicate)));

    std::vector<snapshot::detail::FurniturePlacement> incomplete{
        Furniture(10, "base", "RoomA", 3, -11),
        Furniture(30, "upper", "RoomA", 3, -12)};
    AC_CHECK(static_cast<bool>(
        ui::OverlayRuntimeFurnitureState(incomplete, live)));
    AC_CHECK(incomplete[0].room_id == "RoomA");
    AC_CHECK(incomplete[1].room_id.empty());

    std::vector<snapshot::detail::FurniturePlacement> replaced{
        Furniture(10, "old", "RoomA", 3, -11),
        Furniture(20, "new", "", 0, 0)};
    ui::RuntimeFurnitureState replacement_live{
        .placements = {
            RuntimeFurniture(20, "new", "RoomA", 1, -9)}};
    AC_CHECK(static_cast<bool>(
        ui::OverlayRuntimeFurnitureState(replaced, replacement_live)));
    AC_CHECK(replaced[0].room_id.empty());
    AC_CHECK(replaced[1].room_id == "RoomA");
    AC_CHECK(replaced[1].position_x == 1);
    AC_CHECK(replaced[1].position_y == -9);

    ui::RuntimeFurnitureState lock_runtime{
        .placements = {
            RuntimeFurniture(40, "chair", "RoomA", 1, -9),
            RuntimeFurniture(41, "lamp", "RoomB", 2, -9)}};
    std::vector<snapshot::RoomId> locked_rooms{"RoomA", "RoomB", "RoomC"};
    std::vector<ui::RuntimeFurnitureRoomSignature> lock_signatures;
    ui::LockFurnitureRoom(
        locked_rooms, lock_signatures, lock_runtime, "RoomA");
    ui::LockFurnitureRoom(
        locked_rooms, lock_signatures, lock_runtime, "RoomB");
    ui::LockFurnitureRoom(
        locked_rooms, lock_signatures, lock_runtime, "RoomC");
    AC_CHECK(lock_signatures.size() == 3U);
    std::vector<snapshot::RoomId> invalidated;
    ui::ReconcileLockedFurnitureRooms(
        locked_rooms, lock_signatures, lock_runtime, &invalidated);
    AC_CHECK(invalidated.empty());
    AC_CHECK(locked_rooms.size() == 3U);

    lock_runtime.placements[0].position_x = 3;
    ui::ReconcileLockedFurnitureRooms(
        locked_rooms, lock_signatures, lock_runtime, &invalidated);
    AC_CHECK(invalidated == std::vector<snapshot::RoomId>{"RoomA"});
    AC_CHECK(locked_rooms ==
        std::vector<snapshot::RoomId>({"RoomB", "RoomC"}));

    lock_runtime.placements.clear();
    invalidated.clear();
    ui::ReconcileLockedFurnitureRooms(
        locked_rooms, lock_signatures, lock_runtime, &invalidated);
    AC_CHECK(invalidated ==
        std::vector<snapshot::RoomId>({"RoomB", "RoomC"}));
    AC_CHECK(locked_rooms.empty());
}

}  // namespace autocattery::tests
