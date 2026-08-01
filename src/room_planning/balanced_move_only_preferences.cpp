#include "balanced_move_only_internal.hpp"

#include <tuple>

namespace autocattery::room_planning::balanced_internal {
namespace {

const snapshot::RoomAttributes* Attributes(
    const PlanningContext& context,
    const snapshot::RoomId& room_id) {
    const auto& room = *context.room_snapshots.at(room_id);
    return room.attributes ? &*room.attributes : nullptr;
}

auto OccupancyKey(
    const PlanningContext& context,
    const snapshot::RoomId& room_id,
    const CountMap& target) {
    const auto* attributes = Attributes(context, room_id);
    const auto next_count = target.at(room_id) + 1U;
    const auto crowding = next_count > 4U ? next_count - 4U : 0U;
    const auto effective_comfort = attributes
        ? attributes->comfort - static_cast<double>(crowding)
        : 0.0;
    return std::tuple{
        target.at(room_id),
        attributes ? 0 : 1,
        -effective_comfort,
        attributes ? -attributes->stimulation : 0.0,
        attributes ? -attributes->health : 0.0,
        attributes ? -attributes->mutation : 0.0,
        room_id
    };
}

auto DevelopmentKey(
    const PlanningContext& context,
    const snapshot::RoomId& room_id) {
    const auto* attributes = Attributes(context, room_id);
    return std::tuple{
        attributes ? 0 : 1,
        attributes ? -attributes->stimulation : 0.0,
        attributes ? -attributes->comfort : 0.0,
        attributes ? -attributes->health : 0.0,
        attributes ? -attributes->mutation : 0.0,
        room_id
    };
}

auto BreedingKey(
    const PlanningContext& context,
    const snapshot::RoomId& room_id) {
    const auto* attributes = Attributes(context, room_id);
    const auto primary = attributes
        ? context.breeding_stats_stable
            ? attributes->comfort : attributes->stimulation
        : 0.0;
    const auto secondary = attributes
        ? context.breeding_stats_stable
            ? attributes->stimulation : attributes->comfort
        : 0.0;
    return std::tuple{
        attributes ? 0 : 1,
        -primary,
        -secondary,
        attributes ? -attributes->health : 0.0,
        room_id
    };
}

}  // namespace

bool PreferOccupancyRoom(
    const PlanningContext& context,
    const snapshot::RoomId& left,
    const snapshot::RoomId& right,
    const CountMap& target) {
    return OccupancyKey(context, left, target) <
        OccupancyKey(context, right, target);
}

bool PreferDevelopmentRoom(
    const PlanningContext& context,
    const snapshot::RoomId& left,
    const snapshot::RoomId& right) {
    return DevelopmentKey(context, left) <
        DevelopmentKey(context, right);
}

bool PreferBreedingRoom(
    const PlanningContext& context,
    const snapshot::RoomId& left,
    const snapshot::RoomId& right) {
    return BreedingKey(context, left) < BreedingKey(context, right);
}

}  // namespace autocattery::room_planning::balanced_internal
