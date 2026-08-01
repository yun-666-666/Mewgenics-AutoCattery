#pragma once

#include <cstddef>
#include <unordered_map>
#include <vector>

#include "auto_cattery/room_planning/domain.hpp"

namespace autocattery::room_planning::balanced_internal {

using CountMap = std::unordered_map<snapshot::RoomId, std::size_t>;

enum class SlotSex {
    Any,
    Female,
    Male
};

struct BalancedSlot {
    snapshot::RoomId room_id;
    SlotSex required_sex{SlotSex::Any};
    bool potential_preferred{};
};

struct PlanningContext {
    std::vector<snapshot::RoomId> rooms;
    std::unordered_map<
        snapshot::CatId,
        const snapshot::CatSnapshot*> cats;
    std::unordered_map<
        snapshot::CatId,
        const classification::CatDecision*> decisions;
    std::unordered_map<
        snapshot::RoomId,
        const RoomCapability*> capabilities;
    std::unordered_map<
        snapshot::RoomId,
        const snapshot::RoomSnapshot*> room_snapshots;
    std::vector<snapshot::CatId> movable;
    CountMap current_count;
    CountMap pinned_count;
    CountMap current_potential;
    CountMap current_female;
    CountMap current_male;
    CountMap pinned_potential;
    CountMap pinned_female;
    CountMap pinned_male;
    std::size_t known_female{};
    std::size_t known_male{};
    std::size_t movable_potential{};
};

[[nodiscard]] bool BuildPlanningContext(
    const RoomPlanningInput& input,
    RoomPlan& plan,
    PlanningContext& context);

[[nodiscard]] bool BuildBalancedSlots(
    const PlanningContext& context,
    RoomPlan& plan,
    std::vector<BalancedSlot>& slots);

[[nodiscard]] bool PreferOccupancyRoom(
    const PlanningContext& context,
    const snapshot::RoomId& left,
    const snapshot::RoomId& right,
    const CountMap& target);

[[nodiscard]] bool PreferDevelopmentRoom(
    const PlanningContext& context,
    const snapshot::RoomId& left,
    const snapshot::RoomId& right);

[[nodiscard]] bool AppendMinimumCostMoves(
    const PlanningContext& context,
    const std::vector<BalancedSlot>& slots,
    RoomPlan& plan);

}  // namespace autocattery::room_planning::balanced_internal
