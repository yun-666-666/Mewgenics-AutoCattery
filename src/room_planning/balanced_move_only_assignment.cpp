#include "balanced_move_only_internal.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace autocattery::room_planning::balanced_internal {
namespace {

bool SexMatches(const snapshot::CatSnapshot& cat, SlotSex required) {
    switch (required) {
        case SlotSex::Any:
            return true;
        case SlotSex::Female:
            return cat.sex == snapshot::CatSex::Female;
        case SlotSex::Male:
            return cat.sex == snapshot::CatSex::Male;
    }
    return false;
}

bool IsPotential(const classification::CatDecision& decision) {
    return decision.primary_role ==
        classification::CatRole::CombatRecommended;
}

std::vector<std::size_t> MinimumCostAssignment(
    const std::vector<std::vector<std::int64_t>>& costs) {
    const auto count = costs.size();
    if (count == 0) {
        return {};
    }
    constexpr auto kInfinity =
        std::numeric_limits<std::int64_t>::max() / 8;
    std::vector<std::int64_t> rows(count + 1);
    std::vector<std::int64_t> columns(count + 1);
    std::vector<std::size_t> matched_row(count + 1);
    std::vector<std::size_t> previous(count + 1);

    for (std::size_t row = 1; row <= count; ++row) {
        matched_row[0] = row;
        std::size_t column{};
        std::vector<std::int64_t> minimum(count + 1, kInfinity);
        std::vector<bool> used(count + 1);
        do {
            used[column] = true;
            const auto current_row = matched_row[column];
            auto delta = kInfinity;
            std::size_t next{};
            for (std::size_t candidate = 1;
                 candidate <= count;
                 ++candidate) {
                if (used[candidate]) {
                    continue;
                }
                const auto reduced =
                    costs[current_row - 1][candidate - 1] -
                    rows[current_row] - columns[candidate];
                if (reduced < minimum[candidate]) {
                    minimum[candidate] = reduced;
                    previous[candidate] = column;
                }
                if (minimum[candidate] < delta) {
                    delta = minimum[candidate];
                    next = candidate;
                }
            }
            if (delta >= kInfinity / 2) {
                return {};
            }
            for (std::size_t candidate = 0;
                 candidate <= count;
                 ++candidate) {
                if (used[candidate]) {
                    rows[matched_row[candidate]] += delta;
                    columns[candidate] -= delta;
                } else {
                    minimum[candidate] -= delta;
                }
            }
            column = next;
        } while (matched_row[column] != 0);

        do {
            const auto prior = previous[column];
            matched_row[column] = matched_row[prior];
            column = prior;
        } while (column != 0);
    }

    std::vector<std::size_t> assignment(count, count);
    for (std::size_t column = 1; column <= count; ++column) {
        assignment[matched_row[column] - 1] = column - 1;
    }
    return assignment;
}

}  // namespace

bool AppendMinimumCostMoves(
    const PlanningContext& context,
    const std::vector<BalancedSlot>& slots,
    RoomPlan& plan) {
    constexpr std::int64_t kImpossible =
        std::numeric_limits<std::int64_t>::max() / 16;
    constexpr std::int64_t kRoleMismatchCost = 1'000'000;
    std::vector<std::vector<std::int64_t>> costs(
        context.movable.size(),
        std::vector<std::int64_t>(slots.size(), kImpossible));
    for (std::size_t cat_index = 0;
         cat_index < context.movable.size();
         ++cat_index) {
        const auto& cat = *context.cats.at(context.movable[cat_index]);
        const bool potential =
            IsPotential(*context.decisions.at(cat.id));
        for (std::size_t slot_index = 0;
             slot_index < slots.size();
             ++slot_index) {
            const auto& slot = slots[slot_index];
            if (!SexMatches(cat, slot.required_sex)) {
                continue;
            }
            const auto stable_distance = cat_index > slot_index
                ? cat_index - slot_index
                : slot_index - cat_index;
            costs[cat_index][slot_index] =
                (potential == slot.potential_preferred
                     ? 0
                     : kRoleMismatchCost) +
                static_cast<std::int64_t>(stable_distance);
        }
    }

    const auto assignment = MinimumCostAssignment(costs);
    if (assignment.size() != context.movable.size()) {
        plan.validation_errors.push_back(
            "balanced-room-assignment-infeasible");
        return false;
    }
    for (std::size_t index = 0; index < context.movable.size(); ++index) {
        const auto slot_index = assignment[index];
        if (slot_index >= slots.size() ||
            costs[index][slot_index] >= kImpossible / 2) {
            plan.validation_errors.push_back(
                "balanced-room-assignment-infeasible");
            plan.moves.clear();
            return false;
        }
        const auto& cat = *context.cats.at(context.movable[index]);
        const auto& target = slots[slot_index].room_id;
        if (!cat.room_id || *cat.room_id != target) {
            plan.moves.push_back({
                cat.id,
                cat.room_id.value_or("Outside"),
                target,
                "restore property-ranked cat purpose assignment",
                0,
                true
            });
        }
    }
    std::sort(
        plan.moves.begin(),
        plan.moves.end(),
        [](const auto& left, const auto& right) {
            return left.cat_id < right.cat_id;
        });
    return true;
}

}  // namespace autocattery::room_planning::balanced_internal
