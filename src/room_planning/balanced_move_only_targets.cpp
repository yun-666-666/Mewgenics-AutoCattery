#include "balanced_move_only_internal.hpp"

#include <algorithm>
#include <optional>

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
    for (const auto& room_id : context.rooms) {
        if (target.at(room_id) > RoomCapacity(context, room_id)) {
            plan.limitations.push_back(
                "protected-residents-exceed-configured-room-capacity");
            break;
        }
    }
    if (context.fixed_rooms.size() > context.movable.size()) {
        plan.validation_errors.push_back(
            "fixed-room-count-exceeds-movable-count");
        return false;
    }
    for (std::size_t remaining =
             context.movable.size() - context.fixed_rooms.size();
         remaining > 0;
         --remaining) {
        std::optional<snapshot::RoomId> best;
        for (const auto& room_id : context.rooms) {
            const bool breeding = !context.breeding_pair.empty() && room_id == "Attic";
            if (target[room_id] >= RoomCapacity(context, room_id)) {
                continue;
            }
            if (!best || (breeding && target[room_id] < 2) ||
                (!(context.breeding_pair.size() == 2 && *best == "Attic" &&
                    target[*best] < 2) && PreferOccupancyRoom(
                    context, room_id, *best, target))) {
                best = room_id;
            }
        }
        if (!best) {
            plan.validation_errors.push_back(
                "configured-or-hard-room-capacity-insufficient");
            return false;
        }
        ++target[*best];
    }
    return true;
}

bool IsKitten(const PlanningContext& context, snapshot::CatId cat_id) {
    return context.cats.at(cat_id)->life_stage ==
        snapshot::LifeStage::Kitten;
}

bool IsPotential(const PlanningContext& context, snapshot::CatId cat_id) {
    return !IsKitten(context, cat_id) &&
        std::ranges::find(context.breeding_pair, cat_id) ==
            context.breeding_pair.end() &&
        context.decisions.at(cat_id)->primary_role ==
            classification::CatRole::CombatRecommended;
}

std::size_t RoleCapacity(
    const PlanningContext& context,
    const CountMap& occupancy,
    const snapshot::RoomId& room_id) {
    const auto pinned_roles =
        context.pinned_potential.at(room_id) +
        context.pinned_kitten.at(room_id);
    auto ordinary_residents =
        context.pinned_count.at(room_id) > pinned_roles
            ? context.pinned_count.at(room_id) - pinned_roles
            : 0U;
    for (const auto& [cat_id, fixed_room] : context.fixed_rooms) {
        if (fixed_room == room_id &&
            !IsKitten(context, cat_id) &&
            !IsPotential(context, cat_id)) {
            ++ordinary_residents;
        }
    }
    return occupancy.at(room_id) > ordinary_residents
        ? occupancy.at(room_id) - ordinary_residents
        : 0U;
}

std::size_t FixedPotentialCount(
    const PlanningContext& context,
    const snapshot::RoomId& room_id) {
    return static_cast<std::size_t>(std::ranges::count_if(
        context.fixed_rooms,
        [&](const auto& entry) {
            return entry.second == room_id &&
                IsPotential(context, entry.first);
        }));
}

bool AllocatePotentialTargets(
    const PlanningContext& context,
    const CountMap& occupancy,
    const CountMap& kittens,
    const std::optional<snapshot::RoomId>& breeding_target,
    const std::optional<snapshot::RoomId>& development_target,
    const std::optional<snapshot::RoomId>& kitten_target,
    CountMap& target,
    RoomPlan& plan) {
    target = context.pinned_potential;
    std::size_t fixed_potential{};
    for (const auto& [cat_id, room_id] : context.fixed_rooms) {
        if (IsPotential(context, cat_id)) {
            ++target[room_id];
            ++fixed_potential;
        }
    }
    const auto priority = [&](const snapshot::RoomId& room_id) {
        if (development_target && room_id == *development_target) {
            return 0;
        }
        if (breeding_target && room_id == *breeding_target) {
            return 2;
        }
        if (kitten_target && room_id == *kitten_target) {
            return 3;
        }
        return 1;
    };
    const auto movable_potential = static_cast<std::size_t>(
        std::ranges::count_if(
            context.movable,
            [&](const auto cat_id) {
                return IsPotential(context, cat_id);
            }));
    if (fixed_potential > movable_potential) {
        plan.validation_errors.push_back(
            "fixed-potential-count-exceeds-movable-potential");
        return false;
    }
    for (std::size_t remaining = movable_potential - fixed_potential;
         remaining > 0;
         --remaining) {
        std::optional<snapshot::RoomId> best;
        for (const auto& room_id : context.rooms) {
            if (target[room_id] + kittens.at(room_id) >=
                RoleCapacity(context, occupancy, room_id)) {
                continue;
            }
            if (!best) {
                best = room_id;
                continue;
            }
            const auto room_priority = priority(room_id);
            const auto best_priority = priority(*best);
            if (room_priority != best_priority) {
                if (room_priority < best_priority) {
                    best = room_id;
                }
                continue;
            }
            if (!context.prefer_single_combat_staging_room &&
                target[room_id] != target[*best]) {
                if (target[room_id] < target[*best]) {
                    best = room_id;
                }
                continue;
            }
            if (PreferDevelopmentRoom(context, room_id, *best)) {
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

std::optional<snapshot::RoomId> FindDevelopmentTarget(
    const PlanningContext& context,
    const CountMap& occupancy,
    const std::optional<snapshot::RoomId>& breeding_target) {
    std::optional<snapshot::RoomId> target;
    for (const auto& room_id : context.rooms) {
        if (occupancy.at(room_id) == 0 ||
            (context.config.keep_breeding_pairs_together && room_id == "Attic") ||
            (breeding_target && room_id == *breeding_target)) {
            continue;
        }
        if (!target || PreferDevelopmentRoom(context, room_id, *target)) {
            target = room_id;
        }
    }
    return target;
}

std::optional<snapshot::RoomId> FindKittenTarget(
    const PlanningContext& context,
    const CountMap& occupancy,
    const std::optional<snapshot::RoomId>& breeding_target,
    const std::optional<snapshot::RoomId>& development_target) {
    if (!context.keep_kittens_separate_when_possible ||
        context.movable_kitten == 0) {
        return std::nullopt;
    }
    std::optional<snapshot::RoomId> target;
    for (const auto& room_id : context.rooms) {
        if (occupancy.at(room_id) == 0 ||
            RoleCapacity(context, occupancy, room_id) <=
                context.pinned_kitten.at(room_id) +
                    FixedPotentialCount(context, room_id) ||
            (breeding_target && room_id == *breeding_target) ||
            (development_target && room_id == *development_target)) {
            continue;
        }
        if (!target || PreferKittenRoom(context, room_id, *target)) {
            target = room_id;
        }
    }
    return target;
}

bool AllocateKittenTargets(
    const PlanningContext& context,
    const CountMap& occupancy,
    const std::optional<snapshot::RoomId>& breeding_target,
    const std::optional<snapshot::RoomId>& development_target,
    const std::optional<snapshot::RoomId>& kitten_target,
    CountMap& target,
    RoomPlan& plan) {
    target = context.pinned_kitten;
    std::size_t fixed_kittens{};
    for (const auto& [cat_id, room_id] : context.fixed_rooms) {
        if (IsKitten(context, cat_id)) {
            ++target[room_id];
            ++fixed_kittens;
        }
    }
    if (fixed_kittens > context.movable_kitten) {
        plan.validation_errors.push_back(
            "fixed-kitten-count-exceeds-movable-kittens");
        return false;
    }
    if (!context.keep_kittens_separate_when_possible) {
        return true;
    }
    const auto priority = [&](const snapshot::RoomId& room_id) {
        if (kitten_target && room_id == *kitten_target) {
            return 0;
        }
        if (breeding_target && room_id == *breeding_target) {
            return 3;
        }
        if (development_target && room_id == *development_target) {
            return 2;
        }
        return 1;
    };
    for (std::size_t remaining =
             context.movable_kitten - fixed_kittens;
         remaining > 0;
         --remaining) {
        std::optional<snapshot::RoomId> best;
        for (const auto& room_id : context.rooms) {
            if (target[room_id] +
                    FixedPotentialCount(context, room_id) >=
                RoleCapacity(context, occupancy, room_id)) {
                continue;
            }
            if (!best) {
                best = room_id;
                continue;
            }
            const auto room_priority = priority(room_id);
            const auto best_priority = priority(*best);
            if (room_priority < best_priority ||
                (room_priority == best_priority &&
                 PreferKittenRoom(context, room_id, *best))) {
                best = room_id;
            }
        }
        if (!best) {
            plan.validation_errors.push_back(
                "kitten-room-target-infeasible");
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

bool IsBreedingPairCat(
    const PlanningContext& context,
    snapshot::CatId cat_id) {
    return std::ranges::find(context.breeding_pair, cat_id) !=
        context.breeding_pair.end();
}

void CountKnownSex(
    const snapshot::CatSnapshot& cat,
    std::size_t& female,
    std::size_t& male) {
    female += cat.sex == snapshot::CatSex::Female ? 1U : 0U;
    male += cat.sex == snapshot::CatSex::Male ? 1U : 0U;
}

struct BreedingSexSlots {
    std::size_t female{};
    std::size_t male{};
};

BreedingSexSlots BuildBreedingSexSlots(
    const PlanningContext& context,
    const snapshot::RoomId& room_id,
    std::size_t target_count,
    bool reserve_kittens,
    RoomPlan& plan) {
    std::size_t fixed_target_count{};
    std::size_t mandatory_female = context.pinned_female.at(room_id);
    std::size_t mandatory_male = context.pinned_male.at(room_id);
    for (const auto& [cat_id, fixed_room] : context.fixed_rooms) {
        if (fixed_room != room_id) {
            continue;
        }
        ++fixed_target_count;
        CountKnownSex(
            *context.cats.at(cat_id),
            mandatory_female,
            mandatory_male);
    }

    std::size_t pair_target_count{};
    for (const auto cat_id : context.breeding_pair) {
        if (context.fixed_rooms.contains(cat_id)) {
            continue;
        }
        ++pair_target_count;
        CountKnownSex(
            *context.cats.at(cat_id),
            mandatory_female,
            mandatory_male);
    }

    std::size_t flexible_female{};
    std::size_t flexible_male{};
    for (const auto cat_id : context.movable) {
        if (context.fixed_rooms.contains(cat_id) ||
            IsBreedingPairCat(context, cat_id) ||
            (reserve_kittens && IsKitten(context, cat_id))) {
            continue;
        }
        CountKnownSex(
            *context.cats.at(cat_id),
            flexible_female,
            flexible_male);
    }

    const auto mandatory_count =
        context.pinned_count.at(room_id) +
        fixed_target_count + pair_target_count;
    if (mandatory_count > target_count) {
        AddUnique(
            plan.limitations,
            "protected-residents-limit-breeding-sex-balance");
        return {};
    }
    const auto flexible_slots = target_count - mandatory_count;
    const auto ideal_minimum = std::min({
        target_count / 2U,
        mandatory_female + flexible_female,
        mandatory_male + flexible_male
    });
    auto achievable = ideal_minimum;
    while (achievable > 0) {
        const auto needed_female = achievable > mandatory_female
            ? achievable - mandatory_female
            : 0U;
        const auto needed_male = achievable > mandatory_male
            ? achievable - mandatory_male
            : 0U;
        if (needed_female <= flexible_female &&
            needed_male <= flexible_male &&
            needed_female + needed_male <= flexible_slots) {
            break;
        }
        --achievable;
    }
    if (achievable < ideal_minimum) {
        AddUnique(
            plan.limitations,
            "protected-residents-limit-breeding-sex-balance");
    }
    return {
        achievable > context.pinned_female.at(room_id)
            ? achievable - context.pinned_female.at(room_id)
            : 0U,
        achievable > context.pinned_male.at(room_id)
            ? achievable - context.pinned_male.at(room_id)
            : 0U
    };
}

}  // namespace

bool BuildBalancedSlots(
    const PlanningContext& context,
    RoomPlan& plan,
    std::vector<BalancedSlot>& slots) {
    for (const auto& room_id : context.rooms) {
        if (context.pinned_potential.at(room_id) +
                context.pinned_kitten.at(room_id) >
            context.pinned_count.at(room_id)) {
            plan.validation_errors.push_back(
                "pinned-role-count-exceeds-pinned-residents");
            return false;
        }
    }
    CountMap occupancy;
    CountMap potential;
    CountMap kittens;
    if (!AllocateOccupancyTargets(context, occupancy, plan)) {
        return false;
    }

    const auto breeding_target = FindBreedingTarget(
        context, occupancy, std::nullopt);
    const auto development_target = FindDevelopmentTarget(
        context, occupancy, breeding_target);
    const auto kitten_target = FindKittenTarget(
        context, occupancy, breeding_target, development_target);
    if (!AllocateKittenTargets(
            context,
            occupancy,
            breeding_target,
            development_target,
            kitten_target,
            kittens,
            plan) ||
        !AllocatePotentialTargets(
            context,
            occupancy,
            kittens,
            breeding_target,
            development_target,
            kitten_target,
            potential,
            plan)) {
        return false;
    }

    slots.reserve(context.movable.size());
    for (const auto& room_id : context.rooms) {
        if (occupancy.at(room_id) < context.pinned_count.at(room_id)) {
            plan.validation_errors.push_back(
                "room-occupancy-below-pinned-count");
            return false;
        }
        const auto slot_count =
            occupancy.at(room_id) - context.pinned_count.at(room_id);
        std::vector<SlotSex> requirements;
        if (breeding_target &&
            room_id == *breeding_target) {
            const auto sex_slots = BuildBreedingSexSlots(
                context,
                room_id,
                occupancy.at(room_id),
                kitten_target.has_value(),
                plan);
            requirements.insert(
                requirements.end(),
                sex_slots.female,
                SlotSex::Female);
            requirements.insert(
                requirements.end(),
                sex_slots.male,
                SlotSex::Male);
        }
        if (requirements.size() > slot_count) {
            AddUnique(
                plan.limitations,
                "protected-residents-limit-breeding-sex-balance");
            requirements.clear();
        }
        requirements.resize(slot_count, SlotSex::Any);
        if (potential.at(room_id) < context.pinned_potential.at(room_id) ||
            kittens.at(room_id) < context.pinned_kitten.at(room_id)) {
            plan.validation_errors.push_back(
                "room-role-target-below-pinned-count");
            return false;
        }
        const auto preferred = potential.at(room_id) -
            context.pinned_potential.at(room_id);
        const auto kitten_preferred =
            kitten_target && room_id == *kitten_target
                ? kittens.at(room_id) -
                    context.pinned_kitten.at(room_id)
                : 0U;
        if (preferred + kitten_preferred > slot_count) {
            plan.validation_errors.push_back(
                "room-role-target-count-infeasible");
            return false;
        }
        for (std::size_t index = 0; index < requirements.size(); ++index) {
            slots.push_back({
                room_id,
                requirements[index],
                index >= kitten_preferred &&
                    index < kitten_preferred + preferred,
                index < kitten_preferred,
                false,
                std::nullopt
            });
        }
    }
    if (slots.size() == context.movable.size()) {
        AssignFixedRoomSlots(context, plan, slots);
        AssignBreedingPairSlots(context, breeding_target, plan, slots);
        AssignBreedingPoolSlots(context, breeding_target, plan, slots);
        return true;
    }
    plan.validation_errors.push_back("balanced-room-slot-count-mismatch");
    return false;
}

}  // namespace autocattery::room_planning::balanced_internal
