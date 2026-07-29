#include "auto_cattery/room_planning/planner.hpp"

#include <algorithm>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

#include "auto_cattery/protection/policy.hpp"
#include "auto_cattery/room_planning/validator.hpp"

namespace autocattery::room_planning {
namespace {

using CatMap = std::unordered_map<
    snapshot::CatId,
    const snapshot::CatSnapshot*>;
using DecisionMap = std::unordered_map<
    snapshot::CatId,
    const classification::CatDecision*>;
using ProtectionMap = std::unordered_map<
    snapshot::CatId,
    const protection::ProtectionDecision*>;
using CapabilityMap = std::unordered_map<
    snapshot::RoomId,
    const RoomCapability*>;
using OccupancyMap = std::unordered_map<
    snapshot::RoomId,
    std::vector<snapshot::CatId>>;

void AddUnique(std::vector<std::string>& values, std::string value) {
    if (std::ranges::find(values, value) == values.end()) {
        values.push_back(std::move(value));
    }
}

bool RoomBoundaryConfirmed(const RoomCapability& capability) {
    return capability.confirmed_hard_capacity.has_value() &&
        capability.confirmed_role != RoomRole::Unknown &&
        capability.confirmed_role != RoomRole::Special &&
        capability.confirmed_role != RoomRole::Unavailable &&
        capability.special_room == CapabilityState::No &&
        capability.player_locked == CapabilityState::No &&
        capability.forced_residents_present == CapabilityState::No;
}

bool CanReceive(const RoomCapability& capability) {
    return RoomBoundaryConfirmed(capability) &&
        capability.can_receive_residents == CapabilityState::Yes;
}

bool CanRelease(const RoomCapability& capability) {
    return RoomBoundaryConfirmed(capability) &&
        capability.can_release_residents == CapabilityState::Yes;
}

bool ManagedMovable(
    const snapshot::CatSnapshot& cat,
    const classification::CatDecision& decision,
    const protection::ProtectionDecision& policy) {
    if (cat.in_adventure_box ||
        !policy.automatically_managed ||
        policy.fail_closed ||
        !policy.move_allowed ||
        !decision.move_allowed) {
        return false;
    }
    return decision.primary_role !=
            classification::CatRole::ProtectedUnmanaged &&
        decision.primary_role != classification::CatRole::Ineligible &&
        decision.primary_role != classification::CatRole::BreedingCore;
}

bool RoleMatches(
    const snapshot::CatSnapshot& cat,
    const classification::CatDecision& decision,
    const RoomCapability& room) {
    if (cat.life_stage == snapshot::LifeStage::Kitten) {
        return room.confirmed_role == RoomRole::Kitten ||
            room.confirmed_role == RoomRole::General;
    }
    switch (decision.primary_role) {
        case classification::CatRole::CombatRecommended:
            return room.confirmed_role == RoomRole::CombatStaging ||
                room.confirmed_role == RoomRole::General;
        case classification::CatRole::BreedingReserve:
        case classification::CatRole::GeneralReserve:
        case classification::CatRole::CullCandidate:
            return room.confirmed_role == RoomRole::General;
        default:
            return false;
    }
}

int RolePreference(
    const snapshot::CatSnapshot& cat,
    const classification::CatDecision& decision,
    const RoomCapability& room) {
    if (cat.life_stage == snapshot::LifeStage::Kitten) {
        return room.confirmed_role == RoomRole::Kitten ? 0 : 1;
    }
    if (decision.primary_role ==
        classification::CatRole::CombatRecommended) {
        return room.confirmed_role == RoomRole::CombatStaging ? 0 : 1;
    }
    return 0;
}

int CatPriority(const classification::CatDecision& decision) {
    switch (decision.primary_role) {
        case classification::CatRole::CombatRecommended:
            return 0;
        case classification::CatRole::BreedingReserve:
            return 1;
        case classification::CatRole::GeneralReserve:
            return 2;
        case classification::CatRole::CullCandidate:
            return 3;
        default:
            return 4;
    }
}

std::size_t SoftOverflow(
    std::size_t occupancy,
    const RoomPlanningConfig& config) {
    return occupancy > config.default_soft_capacity
        ? occupancy - config.default_soft_capacity
        : 0;
}

bool HasUnplaced(
    const std::vector<UnplacedCat>& values,
    snapshot::CatId cat_id) {
    return std::ranges::any_of(
        values,
        [cat_id](const auto& value) {
            return value.cat_id == cat_id;
        });
}

}  // namespace

RoomPlan PlanRooms(
    const RoomPlanningInput& input,
    const RoomPlanningConfig& config) {
    RoomPlan plan;
    plan.source_snapshot_id = input.snapshot.snapshot_id;

    if (config.version != 1 ||
        config.default_soft_capacity == 0 ||
        !config.never_exceed_known_hard_capacity ||
        !config.allow_partial_plan) {
        plan.validation_errors.push_back("room-planning-config-invalid");
        return plan;
    }

    const auto validation = ValidateInput(input);
    plan.validation_errors = validation.errors;
    plan.limitations = validation.limitations;
    if (!validation.Valid()) {
        return plan;
    }
    if (protection::Recheck(
            input.preview_protection_digest,
            input.current_protection_digest) ==
        protection::RecheckResult::CancelAndRepreview) {
        plan.disposition = PlanDisposition::CancelAndRepreview;
        plan.warnings.push_back(
            "protection changed; cancel and create a new preview");
        return plan;
    }

    CatMap cats;
    DecisionMap decisions;
    ProtectionMap protections;
    CapabilityMap capabilities;
    OccupancyMap occupancy;
    for (const auto& cat : input.snapshot.cats) {
        cats.emplace(cat.id, &cat);
    }
    for (const auto& decision : input.classification.decisions) {
        decisions.emplace(decision.cat_id, &decision);
    }
    for (const auto& policy : input.protections) {
        protections.emplace(policy.cat_id, &policy);
    }
    for (const auto& capability : input.room_capabilities) {
        capabilities.emplace(capability.room_id, &capability);
    }
    for (const auto& room : input.snapshot.rooms) {
        occupancy.emplace(room.id, room.residents);
    }

    std::vector<snapshot::CatId> candidates;
    candidates.reserve(input.snapshot.cats.size());
    for (const auto& cat : input.snapshot.cats) {
        if (!cat.room_id.has_value()) {
            plan.unplaced_cats.push_back({
                cat.id,
                "current room is missing"
            });
            continue;
        }
        const auto& decision = *decisions.at(cat.id);
        const auto& policy = *protections.at(cat.id);
        if (decision.primary_role ==
            classification::CatRole::BreedingCore) {
            AddUnique(
                plan.limitations,
                "breeding-pair-evidence-unavailable");
            continue;
        }
        if (!ManagedMovable(cat, decision, policy)) {
            continue;
        }
        const auto source = capabilities.find(*cat.room_id);
        if (source == capabilities.end() ||
            !CanRelease(*source->second)) {
            AddUnique(
                plan.limitations,
                "source-room-capability-unknown");
            continue;
        }
        candidates.push_back(cat.id);
    }

    std::sort(
        candidates.begin(),
        candidates.end(),
        [&](const auto left, const auto right) {
            const auto left_priority = CatPriority(*decisions.at(left));
            const auto right_priority = CatPriority(*decisions.at(right));
            return left_priority == right_priority
                ? left < right
                : left_priority < right_priority;
        });

    std::vector<snapshot::CatId> capacity_failures;
    for (const auto cat_id : candidates) {
        const auto& cat = *cats.at(cat_id);
        const auto& decision = *decisions.at(cat_id);
        const auto& current_id = *cat.room_id;
        const auto& current = *capabilities.at(current_id);
        const auto current_capacity = *current.confirmed_hard_capacity;
        const bool current_over_capacity =
            occupancy.at(current_id).size() > current_capacity;
        const bool current_role_matches =
            RoleMatches(cat, decision, current);
        if (!current_over_capacity && current_role_matches) {
            continue;
        }

        using Option = std::tuple<
            int,
            int,
            std::size_t,
            std::size_t,
            snapshot::RoomId>;
        std::optional<Option> best;
        for (const auto& [room_id, capability] : capabilities) {
            if (room_id == current_id ||
                !CanReceive(*capability) ||
                !RoleMatches(cat, decision, *capability)) {
                continue;
            }
            const auto capacity = *capability->confirmed_hard_capacity;
            const auto projected = occupancy.at(room_id).size() + 1;
            if (projected > capacity) {
                continue;
            }
            const auto overflow = SoftOverflow(projected, config);
            if (!config.allow_soft_overflow && overflow > 0) {
                continue;
            }
            const int configured_overflow_penalty =
                overflow > config.max_soft_overflow_per_room ? 1 : 0;
            const auto option = Option{
                RolePreference(cat, decision, *capability),
                configured_overflow_penalty,
                overflow,
                occupancy.at(room_id).size(),
                room_id
            };
            if (!best.has_value() || option < *best) {
                best = option;
            }
        }

        if (!best.has_value()) {
            if (current_over_capacity) {
                capacity_failures.push_back(cat_id);
            } else if (!current_role_matches) {
                AddUnique(
                    plan.limitations,
                    "preferred-room-role-unavailable");
            }
            continue;
        }

        const auto& target_id = std::get<4>(*best);
        auto& source_residents = occupancy.at(current_id);
        source_residents.erase(
            std::remove(
                source_residents.begin(),
                source_residents.end(),
                cat_id),
            source_residents.end());
        occupancy.at(target_id).push_back(cat_id);
        plan.moves.push_back({
            cat_id,
            current_id,
            target_id,
            current_over_capacity
                ? "relieve confirmed hard-capacity overflow"
                : "use confirmed compatible room role",
            CatPriority(decision),
            false
        });
    }

    std::size_t remaining_over_capacity{};
    for (const auto& [room_id, residents] : occupancy) {
        const auto& capability = *capabilities.at(room_id);
        if (!capability.confirmed_hard_capacity.has_value()) {
            AddUnique(plan.limitations, "target-hard-capacity-unknown");
            continue;
        }
        if (residents.size() > *capability.confirmed_hard_capacity) {
            remaining_over_capacity +=
                residents.size() - *capability.confirmed_hard_capacity;
        }
    }
    plan.minimum_capacity_relief_required = remaining_over_capacity;
    for (std::size_t index = 0;
         index < capacity_failures.size() &&
         index < remaining_over_capacity;
         ++index) {
        if (!HasUnplaced(plan.unplaced_cats, capacity_failures[index])) {
            plan.unplaced_cats.push_back({
                capacity_failures[index],
                "confirmed hard capacity has no safe destination"
            });
        }
    }

    for (std::size_t index = 0;
         index < input.classification.capacity_relief_candidates.size() &&
         plan.capacity_relief_suggestions.size() <
             plan.minimum_capacity_relief_required;
         ++index) {
        const auto cat_id =
            input.classification.capacity_relief_candidates[index];
        const auto policy = protections.find(cat_id);
        const auto decision = decisions.find(cat_id);
        if (policy == protections.end() ||
            decision == decisions.end() ||
            policy->second->fail_closed ||
            !policy->second->cull_allowed ||
            !decision->second->preview_cull_candidate) {
            continue;
        }
        plan.capacity_relief_suggestions.push_back({
            cat_id,
            index,
            "read-only candidate for minimum confirmed capacity relief",
            false
        });
    }

    if (plan.minimum_capacity_relief_required > 0) {
        plan.warnings.push_back(
            "confirmed capacity remains insufficient; partial plan");
    }
    if (plan.capacity_relief_suggestions.size() <
        plan.minimum_capacity_relief_required) {
        AddUnique(
            plan.limitations,
            "safe-capacity-relief-candidates-insufficient");
    }

    std::sort(
        plan.moves.begin(),
        plan.moves.end(),
        [](const auto& left, const auto& right) {
            return left.cat_id == right.cat_id
                ? left.to_room < right.to_room
                : left.cat_id < right.cat_id;
        });
    std::sort(
        plan.unplaced_cats.begin(),
        plan.unplaced_cats.end(),
        [](const auto& left, const auto& right) {
            return left.cat_id < right.cat_id;
        });
    std::sort(plan.limitations.begin(), plan.limitations.end());
    plan.fully_satisfied =
        plan.unplaced_cats.empty() &&
        plan.minimum_capacity_relief_required == 0;
    plan.disposition = plan.fully_satisfied
        ? PlanDisposition::Complete
        : PlanDisposition::Partial;
    return plan;
}

}  // namespace autocattery::room_planning
