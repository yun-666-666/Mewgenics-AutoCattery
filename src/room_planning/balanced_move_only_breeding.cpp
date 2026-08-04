#include "balanced_move_only_internal.hpp"

#include <algorithm>

namespace autocattery::room_planning::balanced_internal {
namespace {

bool SexMatches(const snapshot::CatSnapshot& cat, SlotSex required) {
    return required == SlotSex::Any ||
        (required == SlotSex::Female &&
         cat.sex == snapshot::CatSex::Female) ||
        (required == SlotSex::Male && cat.sex == snapshot::CatSex::Male);
}

void AddUnique(std::vector<std::string>& values, std::string value) {
    if (std::ranges::find(values, value) == values.end()) {
        values.push_back(std::move(value));
    }
}

}  // namespace

std::optional<snapshot::RoomId> FindBreedingTarget(
    const PlanningContext& context,
    const CountMap& occupancy,
    const std::optional<snapshot::RoomId>& excluded_room) {
    if (context.breeding_pair.size() != 2) {
        return std::nullopt;
    }
    std::optional<snapshot::RoomId> fixed_target;
    for (const auto cat_id : context.breeding_pair) {
        const auto fixed = context.fixed_rooms.find(cat_id);
        if (fixed == context.fixed_rooms.end()) {
            continue;
        }
        if (fixed_target && *fixed_target != fixed->second) {
            return std::nullopt;
        }
        fixed_target = fixed->second;
    }
    std::optional<snapshot::RoomId> target;
    for (const auto& room_id : context.rooms) {
        if (fixed_target && room_id != *fixed_target) {
            continue;
        }
        if (!fixed_target && excluded_room && room_id == *excluded_room) {
            continue;
        }
        if (occupancy.at(room_id) < 2 ||
            occupancy.at(room_id) - context.pinned_count.at(room_id) < 2) {
            continue;
        }
        if (!target || PreferBreedingRoom(context, room_id, *target)) {
            target = room_id;
        }
    }
    return target;
}

void AssignBreedingPairSlots(
    const PlanningContext& context,
    const std::optional<snapshot::RoomId>& target,
    RoomPlan& plan,
    std::vector<BalancedSlot>& slots) {
    if (context.breeding_pair.size() != 2) {
        return;
    }
    if (!target) {
        AddUnique(plan.limitations, "breeding-pair-room-unavailable");
        return;
    }
    for (const auto cat_id : context.breeding_pair) {
        const auto& cat = *context.cats.at(cat_id);
        const auto existing = std::ranges::find_if(
            slots,
            [&](const auto& candidate) {
                return candidate.preferred_cat &&
                    *candidate.preferred_cat == cat_id;
            });
        if (existing != slots.end()) {
            if (existing->room_id != *target) {
                AddUnique(
                    plan.limitations,
                    "breeding-pair-fixed-rooms-conflict");
            }
            continue;
        }
        const auto slot = std::ranges::find_if(
            slots,
            [&](const auto& candidate) {
                return candidate.room_id == *target &&
                    !candidate.preferred_cat &&
                    SexMatches(cat, candidate.required_sex);
            });
        if (slot == slots.end()) {
            AddUnique(plan.limitations, "breeding-pair-sex-slot-unavailable");
            return;
        }
        slot->preferred_cat = cat_id;
    }
}

}  // namespace autocattery::room_planning::balanced_internal
