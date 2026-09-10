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

void RouteSurplusAdults(
    const balanced_internal::PlanningContext& context, RoomPlan& plan) {
    if (!context.prefer_single_combat_staging_room) {
        return;
    }
    std::unordered_map<snapshot::CatId, snapshot::RoomId> rooms;
    for (const auto& [id, cat] : context.cats) {
        rooms[id] = cat->room_id.value_or("Outside");
    }
    for (const auto& move : plan.moves) {
        rooms[move.cat_id] = move.to_room;
    }
    const auto breeding_room = context.breeding_pair.empty()
        ? std::optional<snapshot::RoomId>{}
        : rooms.at(context.breeding_pair.front());
    std::optional<snapshot::RoomId> combat_room;
    for (const auto& room : context.rooms) {
        if (breeding_room == room ||
            !context.room_snapshots.at(room)->attributes) {
            continue;
        }
        if (!combat_room || balanced_internal::PreferDevelopmentRoom(
                context, room, *combat_room)) {
            combat_room = room;
        }
    }
    if (!combat_room) {
        return;
    }
    auto count = static_cast<std::size_t>(std::ranges::count_if(
        rooms, [&](const auto& entry) { return entry.second == *combat_room; }));
    for (const auto id : context.movable) {
        const auto& cat = *context.cats.at(id);
        const auto& decision = *context.decisions.at(id);
        if ((cat.life_stage != snapshot::LifeStage::Adult &&
             cat.life_stage != snapshot::LifeStage::Senior) ||
            context.fixed_rooms.contains(id) || rooms.at(id) == *combat_room ||
            (breeding_room && rooms.at(id) == *breeding_room) ||
            (!breeding_room && (decision.breeding_core || decision.breeding_reserve))) {
            continue;
        }
        if (count >= balanced_internal::RoomCapacity(context, *combat_room)) {
            AddUnique(plan.limitations, "surplus-adult-combat-room-capacity-unavailable");
            break;
        }
        ++count;
        std::erase_if(plan.moves, [id](const auto& move) { return move.cat_id == id; });
        if (cat.room_id != *combat_room) {
            plan.moves.push_back({id, cat.room_id.value_or("Outside"), *combat_room,
                "surplus-adult-combat-room", 0, true});
        }
    }
    std::ranges::sort(plan.moves, {}, &PlannedMove::cat_id);
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

    RouteSurplusAdults(context, plan);
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
