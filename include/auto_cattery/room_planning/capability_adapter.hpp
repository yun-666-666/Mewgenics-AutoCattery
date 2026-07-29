#pragma once

#include "auto_cattery/room_planning/domain.hpp"

namespace autocattery::room_planning {

[[nodiscard]] std::vector<RoomCapability>
BuildConservativeRoomCapabilities(
    const snapshot::HouseSnapshot& snapshot);

}  // namespace autocattery::room_planning
