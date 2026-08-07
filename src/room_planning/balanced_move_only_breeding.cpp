#include "balanced_move_only_internal.hpp"

#include <algorithm>
#include <limits>
#include <unordered_set>

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
        auto slot = std::ranges::find_if(
            slots,
            [&](const auto& candidate) {
                return candidate.room_id == *target &&
                    !candidate.preferred_cat &&
                    !candidate.kitten_preferred &&
                    SexMatches(cat, candidate.required_sex);
            });
        if (slot == slots.end()) {
            slot = std::ranges::find_if(
                slots,
                [&](const auto& candidate) {
                    return candidate.room_id == *target &&
                        !candidate.preferred_cat &&
                        SexMatches(cat, candidate.required_sex);
                });
        }
        if (slot == slots.end()) {
            AddUnique(plan.limitations, "breeding-pair-sex-slot-unavailable");
            return;
        }
        slot->preferred_cat = cat_id;
    }
}

void AssignBreedingPoolSlots(
    const PlanningContext& context,
    const std::optional<snapshot::RoomId>& target,
    std::vector<BalancedSlot>& slots) {
    if (!target || !context.breeding_pair_preferences) {
        return;
    }

    const auto movable = [&](snapshot::CatId cat_id) {
        return std::ranges::find(context.movable, cat_id) !=
            context.movable.end();
    };
    const auto preferred_in_target = [&](snapshot::CatId cat_id) {
        return std::ranges::any_of(
            slots,
            [&](const auto& slot) {
                return slot.room_id == *target &&
                    slot.preferred_cat &&
                    *slot.preferred_cat == cat_id;
            });
    };
    const auto available = [&](snapshot::CatId cat_id) {
        const auto fixed = context.fixed_rooms.find(cat_id);
        if (fixed != context.fixed_rooms.end()) {
            return fixed->second == *target;
        }
        if (movable(cat_id)) {
            return true;
        }
        const auto cat = context.cats.find(cat_id);
        return cat != context.cats.end() &&
            cat->second->room_id &&
            *cat->second->room_id == *target;
    };
    const auto needs_slot = [&](snapshot::CatId cat_id) {
        return movable(cat_id) && !preferred_in_target(cat_id);
    };
    const auto find_slot = [&context, &slots, &target](
            snapshot::CatId cat_id,
            std::optional<std::size_t> excluded) {
        const auto& cat = *context.cats.at(cat_id);
        auto best = slots.size();
        auto best_rank = std::numeric_limits<int>::max();
        for (std::size_t index = 0; index < slots.size(); ++index) {
            const auto& slot = slots[index];
            if ((excluded && index == *excluded) ||
                slot.room_id != *target ||
                slot.preferred_cat ||
                slot.kitten_preferred ||
                !SexMatches(cat, slot.required_sex)) {
                continue;
            }
            const auto rank =
                (slot.required_sex == SlotSex::Any ? 1 : 0) +
                (slot.potential_preferred ? 2 : 0);
            if (rank < best_rank) {
                best = index;
                best_rank = rank;
            }
        }
        return best;
    };

    std::unordered_set<snapshot::CatId> paired(
        context.breeding_pair.begin(), context.breeding_pair.end());
    for (const auto& pair : *context.breeding_pair_preferences) {
        if (paired.contains(pair.cat_a_id) ||
            paired.contains(pair.cat_b_id) ||
            !available(pair.cat_a_id) ||
            !available(pair.cat_b_id)) {
            continue;
        }
        const bool need_a = needs_slot(pair.cat_a_id);
        const bool need_b = needs_slot(pair.cat_b_id);
        const auto slot_a = need_a
            ? find_slot(pair.cat_a_id, std::nullopt)
            : slots.size();
        if (need_a && slot_a == slots.size()) {
            continue;
        }
        const auto slot_b = need_b
            ? find_slot(
                pair.cat_b_id,
                need_a ? std::optional<std::size_t>{slot_a}
                       : std::nullopt)
            : slots.size();
        if (need_b && slot_b == slots.size()) {
            continue;
        }
        if (need_a) {
            slots[slot_a].preferred_cat = pair.cat_a_id;
            slots[slot_a].breeding_pool_preferred = true;
        }
        if (need_b) {
            slots[slot_b].preferred_cat = pair.cat_b_id;
            slots[slot_b].breeding_pool_preferred = true;
        }
        paired.insert(pair.cat_a_id);
        paired.insert(pair.cat_b_id);
    }
}

}  // namespace autocattery::room_planning::balanced_internal
