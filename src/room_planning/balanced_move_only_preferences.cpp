#include "balanced_move_only_internal.hpp"

#include <algorithm>
#include <limits>
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
        attributes ? attributes->comfort : 0.0,
        attributes ? -attributes->mutation : 0.0,
        attributes ? attributes->health : 0.0,
        attributes ? attributes->stimulation : 0.0,
        room_id
    };
}

auto BreedingKey(
    const PlanningContext& context,
    const snapshot::RoomId& room_id) {
    const auto* attributes = Attributes(context, room_id);
    const auto viable = attributes && attributes->comfort > -10.0;
    const auto balanced_environment = attributes
        ? std::min(attributes->comfort, attributes->stimulation)
        : 0.0;
    return std::tuple{
        attributes ? 0 : 1,
        viable ? 0 : 1,
        attributes ? -balanced_environment : 0.0,
        attributes ? -attributes->comfort : 0.0,
        attributes ? -attributes->stimulation : 0.0,
        attributes && context.breeding_stats_stable
            ? -attributes->mutation : 0.0,
        attributes ? -attributes->health : 0.0,
        room_id
    };
}

auto KittenKey(
    const PlanningContext& context,
    const snapshot::RoomId& room_id) {
    const auto* attributes = Attributes(context, room_id);
    return std::tuple{
        attributes ? 0 : 1,
        attributes && attributes->comfort > -10.0 ? 0 : 1,
        attributes ? -attributes->health : 0.0,
        attributes ? -attributes->comfort : 0.0,
        attributes ? -attributes->stimulation : 0.0,
        room_id
    };
}

}  // namespace

std::size_t RoomCapacity(
    const PlanningContext& context, const snapshot::RoomId& room_id) {
    const auto configured = context.config.allow_soft_overflow
        ? std::numeric_limits<std::size_t>::max()
        : context.config.default_soft_capacity;
    const auto hard = context.capabilities.at(room_id)->confirmed_hard_capacity;
    return hard ? std::min(configured, *hard) : configured;
}

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

bool PreferKittenRoom(
    const PlanningContext& context,
    const snapshot::RoomId& left,
    const snapshot::RoomId& right) {
    return KittenKey(context, left) < KittenKey(context, right);
}

}  // namespace autocattery::room_planning::balanced_internal
