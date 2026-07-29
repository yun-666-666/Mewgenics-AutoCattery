#include "auto_cattery/room_planning/capability_adapter.hpp"

#include "test_support.hpp"

namespace autocattery::tests {

void RunRoomCapabilityAdapterTests() {
    snapshot::HouseSnapshot house;
    house.snapshot_id = 9;
    house.capabilities.read_room_assignments = true;
    house.capabilities.read_room_capacities = true;
    house.cats = {
        {.id = 1, .room_id = "BreedingRoom_4"},
        {.id = 2, .room_id = "SpecialRoom_99"}
    };
    house.rooms = {
        {.id = "SpecialRoom_99", .residents = {2}},
        {.id = "BreedingRoom_4", .residents = {1}}
    };

    const auto capabilities =
        room_planning::BuildConservativeRoomCapabilities(house);
    AC_CHECK(capabilities.size() == 2);
    AC_CHECK(capabilities[0].room_id == "BreedingRoom_4");
    AC_CHECK(capabilities[1].room_id == "SpecialRoom_99");
    for (const auto& capability : capabilities) {
        AC_CHECK(
            capability.confirmed_role ==
            room_planning::RoomRole::Unknown);
        AC_CHECK(!capability.confirmed_hard_capacity.has_value());
        AC_CHECK(
            capability.special_room ==
            room_planning::CapabilityState::Unknown);
        AC_CHECK(
            capability.player_locked ==
            room_planning::CapabilityState::Unknown);
        AC_CHECK(
            capability.forced_residents_present ==
            room_planning::CapabilityState::Unknown);
        AC_CHECK(
            capability.can_receive_residents ==
            room_planning::CapabilityState::Unknown);
        AC_CHECK(
            capability.can_release_residents ==
            room_planning::CapabilityState::Unknown);
    }
}

}  // namespace autocattery::tests
