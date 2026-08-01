#include "balanced_move_only_internal.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <tuple>
#include <unordered_set>

namespace autocattery::room_planning::balanced_internal {
namespace {

bool AllocateOccupancyTargets(
    const PlanningContext& context,
    CountMap& target,
    RoomPlan& plan) {
    target = context.pinned_count;
    for (const auto& [cat_id, room_id] : context.fixed_rooms) {
        static_cast<void>(cat_id);
        const auto& capability = *context.capabilities.at(room_id);
        if (capability.confirmed_hard_capacity &&
            target[room_id] >= *capability.confirmed_hard_capacity) {
            plan.validation_errors.push_back(
                "fixed-protection-room-capacity-insufficient");
            return false;
        }
        ++target[room_id];
    }
    for (std::size_t remaining =
             context.movable.size() - context.fixed_rooms.size();
         remaining > 0;
         --remaining) {
        std::optional<snapshot::RoomId> best;
        for (const auto& room_id : context.rooms) {
            const auto& capability = *context.capabilities.at(room_id);
            if (capability.confirmed_hard_capacity &&
                target[room_id] >= *capability.confirmed_hard_capacity) {
                continue;
            }
            if (!best ||
                PreferOccupancyRoom(
                    context, room_id, *best, target)) {
                best = room_id;
            }
        }
        if (!best) {
            plan.validation_errors.push_back(
                "confirmed-room-capacity-insufficient");
            return false;
        }
        ++target[*best];
    }
    return true;
}

bool AllocatePotentialTargets(
    const PlanningContext& context,
    const CountMap& occupancy,
    CountMap& target,
    RoomPlan& plan) {
    target = context.pinned_potential;
    for (std::size_t remaining = context.movable_potential;
         remaining > 0;
         --remaining) {
        std::optional<snapshot::RoomId> best;
        for (const auto& room_id : context.rooms) {
            if (target[room_id] >= occupancy.at(room_id)) {
                continue;
            }
            if (!best ||
                PreferDevelopmentRoom(context, room_id, *best)) {
                best = room_id;
            }
        }
        if (!best) {
            plan.validation_errors.push_back(
                "potential-room-target-infeasible");
            return false;
        }
        ++target[*best];
    }
    return true;
}

void AddUnique(std::vector<std::string>& values, std::string value) {
    if (std::ranges::find(values, value) == values.end()) {
        values.push_back(std::move(value));
    }
}

std::unordered_set<snapshot::RoomId> RequiredSexRooms(
    const PlanningContext& context,
    const CountMap& occupancy,
    const CountMap& pinned,
    std::size_t known_count,
    const std::optional<snapshot::RoomId>& preferred_room) {
    std::unordered_set<snapshot::RoomId> required;
    if (preferred_room) {
        required.insert(*preferred_room);
    }
    for (const auto& room_id : context.rooms) {
        if (occupancy.at(room_id) >= 2 && pinned.at(room_id) > 0) {
            required.insert(room_id);
        }
    }
    const auto eligible = std::ranges::count_if(
        context.rooms,
        [&](const auto& room_id) {
            return occupancy.at(room_id) >= 2;
        });
    const auto desired = std::min(
        known_count, static_cast<std::size_t>(eligible));
    while (required.size() < desired) {
        std::optional<snapshot::RoomId> best;
        for (const auto& room_id : context.rooms) {
            if (occupancy.at(room_id) < 2 || required.contains(room_id)) {
                continue;
            }
            if (!best ||
                PreferDevelopmentRoom(context, room_id, *best)) {
                best = room_id;
            }
        }
        if (!best) {
            break;
        }
        required.insert(*best);
    }
    return required;
}

}  // namespace

bool BuildBalancedSlots(
    const PlanningContext& context,
    RoomPlan& plan,
    std::vector<BalancedSlot>& slots) {
    CountMap occupancy;
    CountMap potential;
    if (!AllocateOccupancyTargets(context, occupancy, plan) ||
        !AllocatePotentialTargets(
            context, occupancy, potential, plan)) {
        return false;
    }

    const auto breeding_target = FindBreedingTarget(context, occupancy);

    const auto female_rooms = RequiredSexRooms(
        context, occupancy, context.pinned_female, context.known_female,
        BreedingPairHasSex(context, snapshot::CatSex::Female)
            ? breeding_target : std::nullopt);
    const auto male_rooms = RequiredSexRooms(
        context, occupancy, context.pinned_male, context.known_male,
        BreedingPairHasSex(context, snapshot::CatSex::Male)
            ? breeding_target : std::nullopt);
    slots.reserve(context.movable.size());
    for (const auto& room_id : context.rooms) {
        const auto slot_count =
            occupancy.at(room_id) - context.pinned_count.at(room_id);
        std::vector<SlotSex> requirements;
        if (female_rooms.contains(room_id) &&
            context.pinned_female.at(room_id) == 0) {
            requirements.push_back(SlotSex::Female);
        }
        if (male_rooms.contains(room_id) &&
            context.pinned_male.at(room_id) == 0) {
            requirements.push_back(SlotSex::Male);
        }
        if (requirements.size() > slot_count) {
            AddUnique(plan.limitations, "pinned-residents-block-sex-mix");
            requirements.resize(slot_count);
        }
        requirements.resize(slot_count, SlotSex::Any);
        const auto preferred =
            potential.at(room_id) - context.pinned_potential.at(room_id);
        for (std::size_t index = 0; index < requirements.size(); ++index) {
            slots.push_back({
                room_id,
                requirements[index],
                index < preferred,
                std::nullopt
            });
        }
    }
    if (slots.size() == context.movable.size()) {
        AssignFixedRoomSlots(context, plan, slots);
        AssignBreedingPairSlots(context, breeding_target, plan, slots);
        return true;
    }
    plan.validation_errors.push_back("balanced-room-slot-count-mismatch");
    return false;
}

}  // namespace autocattery::room_planning::balanced_internal
