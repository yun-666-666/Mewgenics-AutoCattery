#include "balanced_move_only_internal.hpp"

#include <algorithm>
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

bool IsPotential(const classification::CatDecision& decision) {
    return decision.primary_role ==
        classification::CatRole::CombatRecommended;
}

void InitializeRoomCounts(PlanningContext& context) {
    for (const auto& room_id : context.rooms) {
        context.current_count[room_id] = 0;
        context.pinned_count[room_id] = 0;
        context.current_potential[room_id] = 0;
        context.current_female[room_id] = 0;
        context.current_male[room_id] = 0;
        context.pinned_potential[room_id] = 0;
        context.pinned_female[room_id] = 0;
        context.pinned_male[room_id] = 0;
    }
}

}  // namespace

bool BuildPlanningContext(
    const RoomPlanningInput& input,
    RoomPlan& plan,
    PlanningContext& context) {
    std::unordered_map<
        snapshot::CatId,
        const protection::ProtectionDecision*> protections;
    for (const auto& cat : input.snapshot.cats) {
        context.cats.emplace(cat.id, &cat);
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
    std::sort(context.rooms.begin(), context.rooms.end());
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
        const bool potential = IsPotential(decision);
        const bool movable = ManagedMovable(
            cat, decision, *protections.at(cat.id));
        if (!cat.room_id) {
            if (movable) {
                context.movable.push_back(cat.id);
                context.movable_potential += potential ? 1U : 0U;
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
            continue;
        }
        ++context.pinned_count[room_id];
        context.pinned_potential[room_id] += potential ? 1U : 0U;
        context.pinned_female[room_id] +=
            cat.sex == snapshot::CatSex::Female ? 1U : 0U;
        context.pinned_male[room_id] +=
            cat.sex == snapshot::CatSex::Male ? 1U : 0U;
    }
    std::sort(context.movable.begin(), context.movable.end());
    return true;
}

}  // namespace autocattery::room_planning::balanced_internal
