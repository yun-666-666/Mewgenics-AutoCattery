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
            IsBreedingPairCat(context, cat_id)) {
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
    CountMap occupancy;
    CountMap potential;
    if (!AllocateOccupancyTargets(context, occupancy, plan) ||
        !AllocatePotentialTargets(
            context, occupancy, potential, plan)) {
        return false;
    }

    std::optional<snapshot::RoomId> development_target;
    for (const auto& room_id : context.rooms) {
        if (occupancy.at(room_id) == 0) {
            continue;
        }
        if (!development_target || PreferDevelopmentRoom(
                context, room_id, *development_target)) {
            development_target = room_id;
        }
    }
    const auto breeding_target = FindBreedingTarget(
        context,
        occupancy,
        context.rooms.size() > 1 ? development_target : std::nullopt);

    slots.reserve(context.movable.size());
    for (const auto& room_id : context.rooms) {
        const auto slot_count =
            occupancy.at(room_id) - context.pinned_count.at(room_id);
        std::vector<SlotSex> requirements;
        if (breeding_target &&
            room_id == *breeding_target) {
            const auto sex_slots = BuildBreedingSexSlots(
                context,
                room_id,
                occupancy.at(room_id),
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
