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

}  // namespace autocattery::room_planning
