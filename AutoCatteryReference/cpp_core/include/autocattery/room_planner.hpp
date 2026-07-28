#pragma once
#include "domain.hpp"
namespace autocattery {
RoomPlan PlanRooms(const HouseSnapshot&, const ClassificationPlan&, const RoomPlanningConfig&);
}
