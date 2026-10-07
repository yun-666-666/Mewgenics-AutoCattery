#include "balanced_move_only_internal.hpp"

#include <algorithm>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace autocattery::room_planning::balanced_internal {
namespace {

bool BoundaryConfirmed(const RoomCapability& capability) {
    return capability.confirmed_role == RoomRole::General &&
        capability.special_room == CapabilityState::No &&
        capability.player_locked == CapabilityState::No &&
        capability.forced_residents_present == CapabilityState::No &&
        capability.can_receive_residents == CapabilityState::Yes &&
        capability.can_release_residents == CapabilityState::Yes &&
        capability.native_capacity_gate == CapabilityState::Yes;
}

bool ManagedMovable(
    const snapshot::CatSnapshot& cat,
    const classification::CatDecision& decision,
    const protection::ProtectionDecision& policy) {
    return !cat.in_adventure_box &&
        policy.automatically_managed &&
        !policy.fail_closed &&
        policy.move_allowed &&
        decision.move_allowed &&
        decision.primary_role !=
            classification::CatRole::ProtectedUnmanaged &&
        decision.primary_role != classification::CatRole::Ineligible;
}

bool IsKitten(const snapshot::CatSnapshot& cat) {
    return cat.life_stage == snapshot::LifeStage::Kitten;
}

bool IsPotential(
    const snapshot::CatSnapshot& cat,
    const classification::CatDecision& decision) {
    return !IsKitten(cat) && decision.primary_role ==
        classification::CatRole::CombatRecommended;
}

bool IsKnownOppositeSexPair(
    const snapshot::CatSnapshot& left,
    const snapshot::CatSnapshot& right) {
    return
        (left.sex == snapshot::CatSex::Female &&
         right.sex == snapshot::CatSex::Male) ||
        (left.sex == snapshot::CatSex::Male &&
         right.sex == snapshot::CatSex::Female);
}

void InitializeRoomCounts(PlanningContext& context) {
    for (const auto& room_id : context.rooms) {
        context.current_count[room_id] = 0;
        context.pinned_count[room_id] = 0;
        context.current_potential[room_id] = 0;
        context.current_female[room_id] = 0;
        context.current_male[room_id] = 0;
        context.pinned_potential[room_id] = 0;
        context.pinned_kitten[room_id] = 0;
        context.pinned_female[room_id] = 0;
        context.pinned_male[room_id] = 0;
    }
}

auto RoomPurposeKey(
    const PlanningContext& context,
    const snapshot::RoomId& room_id) {
    const auto* attributes =
        context.room_snapshots.at(room_id)->attributes
            ? &*context.room_snapshots.at(room_id)->attributes
            : nullptr;
    return std::tuple{
        attributes ? 0 : 1,
        attributes ? -attributes->stimulation : 0.0,
        attributes ? -attributes->comfort : 0.0,
        attributes ? -attributes->health : 0.0,
        attributes ? -attributes->mutation : 0.0,
        room_id
    };
}

}  // namespace

bool BuildPlanningContext(
    const RoomPlanningInput& input,
    const RoomPlanningConfig& config,
    RoomPlan& plan,
    PlanningContext& context) {
    context.config = config;
    context.prefer_single_combat_staging_room =
        config.prefer_single_combat_staging_room;
    context.keep_kittens_separate_when_possible =
        config.keep_kittens_separate_when_possible;
    context.breeding_pair_preferences =
        &input.classification.breeding_pair_preferences;
    context.pedigree_pairs = input.snapshot.pedigree_pair_coefficients;
    std::unordered_map<
        snapshot::CatId,
        const protection::ProtectionDecision*> protections;
    for (const auto& cat : input.snapshot.cats) {
        context.cats.emplace(cat.id, &cat);
    }
    for (const auto& room : input.snapshot.rooms) {
        context.room_snapshots.emplace(room.id, &room);
    }
    for (const auto& decision : input.classification.decisions) {
        context.decisions.emplace(decision.cat_id, &decision);
    }
    for (const auto& policy : input.protections) {
        protections.emplace(policy.cat_id, &policy);
    }
    for (const auto& capability : input.room_capabilities) {
        context.capabilities.emplace(capability.room_id, &capability);
        if (BoundaryConfirmed(capability)) {
            context.rooms.push_back(capability.room_id);
        }
    }
    std::sort(
        context.rooms.begin(), context.rooms.end(),
        [&context](const auto& left, const auto& right) {
            return RoomPurposeKey(context, left) <
                RoomPurposeKey(context, right);
        });
    if (context.rooms.empty()) {
        plan.validation_errors.push_back(
            "current-build-move-rooms-unavailable");
        return false;
    }

    InitializeRoomCounts(context);
    const std::unordered_set<snapshot::RoomId> usable(
        context.rooms.begin(), context.rooms.end());
    for (const auto& cat : input.snapshot.cats) {
        const auto& decision = *context.decisions.at(cat.id);
        const bool potential = IsPotential(cat, decision);
        const bool kitten = IsKitten(cat);
        const bool movable = ManagedMovable(
            cat, decision, *protections.at(cat.id));
        const auto& policy = *protections.at(cat.id);
        if (policy.fixed_room && !usable.contains(*policy.fixed_room)) {
            plan.validation_errors.push_back(
                "fixed-protection-room-unavailable");
            return false;
        }
        if (movable && policy.fixed_room) {
            context.fixed_rooms.emplace(cat.id, *policy.fixed_room);
        }
        if (!cat.room_id) {
            if (movable) {
                context.movable.push_back(cat.id);
                context.movable_potential += potential ? 1U : 0U;
                context.movable_kitten += kitten ? 1U : 0U;
                context.known_female +=
                    cat.sex == snapshot::CatSex::Female ? 1U : 0U;
                context.known_male +=
                    cat.sex == snapshot::CatSex::Male ? 1U : 0U;
            }
            continue;
        }
        if (!cat.room_id || !usable.contains(*cat.room_id)) {
            continue;
        }
        const auto& room_id = *cat.room_id;
        ++context.current_count[room_id];
        context.known_female +=
            cat.sex == snapshot::CatSex::Female ? 1U : 0U;
        context.known_male +=
            cat.sex == snapshot::CatSex::Male ? 1U : 0U;
        context.current_female[room_id] +=
            cat.sex == snapshot::CatSex::Female ? 1U : 0U;
        context.current_male[room_id] +=
            cat.sex == snapshot::CatSex::Male ? 1U : 0U;
        context.current_potential[room_id] += potential ? 1U : 0U;
        if (movable) {
            context.movable.push_back(cat.id);
            context.movable_potential += potential ? 1U : 0U;
            context.movable_kitten += kitten ? 1U : 0U;
            continue;
        }
        ++context.pinned_count[room_id];
        context.pinned_potential[room_id] += potential ? 1U : 0U;
        context.pinned_kitten[room_id] += kitten ? 1U : 0U;
        context.pinned_female[room_id] +=
            cat.sex == snapshot::CatSex::Female ? 1U : 0U;
        context.pinned_male[room_id] +=
            cat.sex == snapshot::CatSex::Male ? 1U : 0U;
    }
    std::sort(
        context.movable.begin(),
        context.movable.end(),
        [&context](snapshot::CatId left, snapshot::CatId right) {
            const auto& left_decision = *context.decisions.at(left);
            const auto& right_decision = *context.decisions.at(right);
            const bool left_kitten = IsKitten(*context.cats.at(left));
            const bool right_kitten = IsKitten(*context.cats.at(right));
            if (left_kitten != right_kitten) {
                return !left_kitten;
            }
            const bool left_potential = IsPotential(
                *context.cats.at(left), left_decision);
            const bool right_potential = IsPotential(
                *context.cats.at(right), right_decision);
            if (left_potential != right_potential) {
                return left_potential;
            }
            if (left_decision.combat_score != right_decision.combat_score) {
                return left_decision.combat_score >
                    right_decision.combat_score;
            }
            return left < right;
        });
    if (!config.keep_breeding_pairs_together) {
        return true;
    }
    const std::unordered_set<snapshot::CatId> movable_ids(
        context.movable.begin(), context.movable.end());
    for (const auto id : context.movable) {
        const auto partner = context.decisions.at(id)->breeding_partner_id;
        if (!partner || id >= *partner || !movable_ids.contains(*partner)) {
            continue;
        }
        const auto reciprocal =
            context.decisions.at(*partner)->breeding_partner_id;
        if (reciprocal && *reciprocal == id &&
            IsKnownOppositeSexPair(
                *context.cats.at(id),
                *context.cats.at(*partner))) {
            context.breeding_pair = {id, *partner};
            context.breeding_stats_stable =
                context.decisions.at(id)->breeding_stats_stable;
            break;
        }
    }
    return true;
}

}  // namespace autocattery::room_planning::balanced_internal
