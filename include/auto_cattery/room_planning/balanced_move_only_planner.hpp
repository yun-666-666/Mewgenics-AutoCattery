#pragma once

#include "auto_cattery/room_planning/domain.hpp"

namespace autocattery::room_planning {

[[nodiscard]] RoomPlan PlanCurrentBuildBalancedMoveOnlyRooms(
    const RoomPlanningInput& input,
    const RoomPlanningConfig& config);

}  // namespace autocattery::room_planning
