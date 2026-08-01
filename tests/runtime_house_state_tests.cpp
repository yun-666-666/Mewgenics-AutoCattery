#include "runtime_house_state.hpp"
#include "runtime_house_move_gateway.hpp"

#include <algorithm>

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

}  // namespace

void RunRuntimeHouseStateTests() {
    room_planning::RoomPlan no_op;
    no_op.fully_satisfied = true;
    no_op.disposition = room_planning::PlanDisposition::Complete;
    AC_CHECK(ui::IsRuntimeMovePlanApproved(no_op));
    no_op.validation_errors.push_back("synthetic-error");
    AC_CHECK(!ui::IsRuntimeMovePlanApproved(no_op));

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
}

}  // namespace autocattery::tests
