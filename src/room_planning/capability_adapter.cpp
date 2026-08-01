#include "auto_cattery/room_planning/capability_adapter.hpp"

#include <algorithm>

namespace autocattery::room_planning {

std::vector<RoomCapability> BuildConservativeRoomCapabilities(
    const snapshot::HouseSnapshot& snapshot) {
    std::vector<RoomCapability> capabilities;
    capabilities.reserve(snapshot.rooms.size());
    for (const auto& room : snapshot.rooms) {
        capabilities.push_back({.room_id = room.id});
    }
    std::sort(
        capabilities.begin(),
        capabilities.end(),
        [](const auto& left, const auto& right) {
            return left.room_id < right.room_id;
        });
    return capabilities;
}

std::vector<RoomCapability> BuildCurrentBuildMoveRoomCapabilities(
    const snapshot::HouseSnapshot& snapshot) {
    auto capabilities = BuildConservativeRoomCapabilities(snapshot);
    for (auto& capability : capabilities) {
        if (capability.room_id != "Floor1_Large" &&
            capability.room_id != "Floor1_Small" &&
            capability.room_id != "Floor2_Large" &&
            capability.room_id != "Attic") {
            continue;
        }
        capability.confirmed_role = RoomRole::General;
        capability.special_room = CapabilityState::No;
        capability.player_locked = CapabilityState::No;
        capability.forced_residents_present = CapabilityState::No;
        capability.can_receive_residents = CapabilityState::Yes;
        capability.can_release_residents = CapabilityState::Yes;
        capability.native_capacity_gate = CapabilityState::Yes;
    }
    return capabilities;
}

}  // namespace autocattery::room_planning
