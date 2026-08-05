#include "balanced_move_only_internal.hpp"

#include <algorithm>
#include <tuple>

namespace autocattery::room_planning::balanced_internal {
namespace {

bool SexMatches(const snapshot::CatSnapshot& cat, SlotSex required) {
    return required == SlotSex::Any ||
        (required == SlotSex::Female &&
         cat.sex == snapshot::CatSex::Female) ||
        (required == SlotSex::Male &&
         cat.sex == snapshot::CatSex::Male);
}

void AddUnique(std::vector<std::string>& values, std::string value) {
    if (std::ranges::find(values, value) == values.end()) {
        values.push_back(std::move(value));
    }
}

}  // namespace

void AssignFixedRoomSlots(
    const PlanningContext& context,
    RoomPlan& plan,
    std::vector<BalancedSlot>& slots) {
    for (const auto& [cat_id, room_id] : context.fixed_rooms) {
        const auto& cat = *context.cats.at(cat_id);
        const auto& decision = *context.decisions.at(cat_id);
        const bool kitten =
            cat.life_stage == snapshot::LifeStage::Kitten;
        const bool breeding_pair =
            std::ranges::find(context.breeding_pair, cat_id) !=
            context.breeding_pair.end();
        const bool potential = !kitten && !breeding_pair &&
            decision.primary_role ==
            classification::CatRole::CombatRecommended;
        auto find_best = [&](bool require_sex) {
            auto best = slots.end();
            for (auto candidate = slots.begin();
                 candidate != slots.end();
                 ++candidate) {
                if (candidate->room_id != room_id ||
                    candidate->preferred_cat ||
                    (require_sex &&
                     !SexMatches(cat, candidate->required_sex))) {
                    continue;
                }
                const auto key = std::tuple{
                    candidate->kitten_preferred == kitten ? 0 : 1,
                    candidate->potential_preferred == potential ? 0 : 1
                };
                if (best == slots.end()) {
                    best = candidate;
                    continue;
                }
                const auto best_key = std::tuple{
                    best->kitten_preferred == kitten ? 0 : 1,
                    best->potential_preferred == potential ? 0 : 1
                };
                if (key < best_key) {
                    best = candidate;
                }
            }
            return best;
        };
        auto slot = find_best(true);
        if (slot == slots.end()) {
            slot = find_best(false);
            if (slot != slots.end()) {
                slot->required_sex = SlotSex::Any;
                AddUnique(
                    plan.limitations,
                    "fixed-room-overrides-sex-mix");
            }
        }
        if (slot == slots.end()) {
            plan.validation_errors.push_back(
                "fixed-protection-room-slot-unavailable");
            return;
        }
        slot->preferred_cat = cat_id;
    }
}

}  // namespace autocattery::room_planning::balanced_internal
