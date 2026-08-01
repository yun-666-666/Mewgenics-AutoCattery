#include "balanced_move_only_internal.hpp"

#include <algorithm>

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
        auto slot = std::ranges::find_if(
            slots,
            [&](const auto& candidate) {
                return candidate.room_id == room_id &&
                    !candidate.preferred_cat &&
                    SexMatches(cat, candidate.required_sex);
            });
        if (slot == slots.end()) {
            slot = std::ranges::find_if(
                slots,
                [&](const auto& candidate) {
                    return candidate.room_id == room_id &&
                        !candidate.preferred_cat;
                });
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
