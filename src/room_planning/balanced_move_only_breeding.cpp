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
    const CountMap& occupancy) {
    if (context.breeding_pair.size() != 2) {
        return std::nullopt;
    }
    std::optional<snapshot::RoomId> target;
    for (const auto& room_id : context.rooms) {
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

bool BreedingPairHasSex(
    const PlanningContext& context,
    snapshot::CatSex sex) {
    return std::ranges::any_of(
        context.breeding_pair,
        [&](snapshot::CatId id) {
            return context.cats.at(id)->sex == sex;
        });
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
        const auto slot = std::ranges::find_if(
            slots,
            [&](const auto& candidate) {
                return candidate.room_id == *target &&
                    !candidate.preferred_cat &&
                    SexMatches(cat, candidate.required_sex);
            });
        if (slot == slots.end()) {
            AddUnique(plan.limitations, "breeding-pair-sex-slot-unavailable");
            for (auto& candidate : slots) {
                if (candidate.preferred_cat &&
                    std::ranges::find(
                        context.breeding_pair,
                        *candidate.preferred_cat) !=
                        context.breeding_pair.end()) {
                    candidate.preferred_cat.reset();
                }
            }
            return;
        }
        slot->preferred_cat = cat_id;
    }
}

}  // namespace autocattery::room_planning::balanced_internal
