#include "auto_cattery/room_planning/balanced_move_only_planner.hpp"

#include <algorithm>

#include "auto_cattery/protection/policy.hpp"
#include "auto_cattery/room_planning/validator.hpp"
#include "balanced_move_only_internal.hpp"

namespace autocattery::room_planning {
namespace {

void AddUnique(std::vector<std::string>& values, std::string value) {
    if (std::ranges::find(values, value) == values.end()) {
        values.push_back(std::move(value));
    }
}

}  // namespace

RoomPlan PlanCurrentBuildBalancedMoveOnlyRooms(
    const RoomPlanningInput& input,
    const RoomPlanningConfig& config) {
    RoomPlan plan;
    plan.source_snapshot_id = input.snapshot.snapshot_id;
    plan.algorithm_version = kBalancedMoveOnlyAlgorithmVersion;
    if (config.version != 1 || config.default_soft_capacity == 0 ||
        config.default_soft_capacity > 1000 ||
        config.max_soft_overflow_per_room > 1000 ||
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

    balanced_internal::PlanningContext context;
    std::vector<balanced_internal::BalancedSlot> slots;
    if (!balanced_internal::BuildPlanningContext(
            input, config, plan, context) ||
        !balanced_internal::BuildBalancedSlots(
            context, plan, slots) ||
        !balanced_internal::AppendMinimumCostMoves(
            context, slots, plan)) {
        return plan;
    }

    if (!input.snapshot.capabilities.read_room_attributes) {
        AddUnique(plan.limitations, "room-attributes-unavailable");
    }
    if (config.allow_soft_overflow) {
        auto occupancy = context.current_count;
        for (const auto& move : plan.moves) {
            if (occupancy.contains(move.from_room)) {
                --occupancy.at(move.from_room);
            }
            ++occupancy[move.to_room];
        }
        for (const auto& [room, count] : occupancy) {
            if (count > config.default_soft_capacity +
                    config.max_soft_overflow_per_room) {
                AddUnique(plan.limitations, "room-crowding-advisory-threshold-exceeded");
                break;
            }
        }
    }
    if (!input.snapshot.capabilities.read_sexuality ||
        !input.snapshot.capabilities.read_relationships) {
        AddUnique(plan.limitations, "unlocked-breeding-fields-unavailable");
    } else if (context.breeding_pair.empty()) {
        AddUnique(plan.limitations, "eligible-breeding-pair-unavailable");
    }
    plan.fully_satisfied = true;
    plan.disposition = PlanDisposition::Complete;
    plan.move_execution_allowed = !plan.moves.empty();
    plan.cull_execution_allowed = false;
    return plan;
}

}  // namespace autocattery::room_planning
