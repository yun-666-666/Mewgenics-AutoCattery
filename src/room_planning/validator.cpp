#include "auto_cattery/room_planning/validator.hpp"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#include "auto_cattery/protection/policy.hpp"

namespace autocattery::room_planning {
namespace {

void AddUnique(std::vector<std::string>& values, std::string value) {
    if (std::ranges::find(values, value) == values.end()) {
        values.push_back(std::move(value));
    }
}

}  // namespace

PlanningValidation ValidateInput(const RoomPlanningInput& input) {
    PlanningValidation validation;
    const auto& house = input.snapshot;

    if (house.snapshot_id == 0) {
        AddUnique(validation.errors, "snapshot-id-missing");
    }
    if (!snapshot::Validate(house).Valid()) {
        AddUnique(validation.errors, "snapshot-invalid");
    }
    if (input.classification.source_snapshot_id != house.snapshot_id) {
        AddUnique(validation.errors, "classification-snapshot-mismatch");
    }

    std::unordered_set<snapshot::CatId> cat_ids;
    for (const auto& cat : house.cats) {
        cat_ids.insert(cat.id);
    }

    std::unordered_set<snapshot::CatId> decision_ids;
    for (const auto& decision : input.classification.decisions) {
        if (!cat_ids.contains(decision.cat_id)) {
            AddUnique(validation.errors, "classification-unknown-cat");
        }
        if (!decision_ids.insert(decision.cat_id).second) {
            AddUnique(validation.errors, "classification-duplicate-cat");
        }
    }
    if (decision_ids.size() != cat_ids.size()) {
        AddUnique(validation.errors, "classification-missing-cat");
    }

    std::unordered_map<snapshot::CatId, const protection::ProtectionDecision*>
        protections;
    for (const auto& decision : input.protections) {
        if (!cat_ids.contains(decision.cat_id)) {
            AddUnique(validation.errors, "protection-unknown-cat");
        }
        if (!protections.emplace(decision.cat_id, &decision).second) {
            AddUnique(validation.errors, "protection-duplicate-cat");
        }
    }
    if (protections.size() != cat_ids.size()) {
        AddUnique(validation.errors, "protection-missing-cat");
    }
    if (protection::BuildDigest(input.protections) !=
        input.current_protection_digest) {
        AddUnique(
            validation.errors,
            "current-protection-digest-mismatch");
    }

    for (const auto& decision : input.classification.decisions) {
        const auto protection = protections.find(decision.cat_id);
        if (protection == protections.end()) {
            continue;
        }
        const auto& policy = *protection->second;
        if (decision.protection_level != policy.effective_level ||
            decision.move_allowed != policy.move_allowed ||
            decision.blacklist_preferred != policy.blacklist_preferred) {
            AddUnique(validation.errors, "protection-classification-mismatch");
        }
    }

    std::unordered_set<snapshot::RoomId> room_ids;
    for (const auto& room : house.rooms) {
        room_ids.insert(room.id);
        std::unordered_set<snapshot::CatId> residents;
        for (const auto cat_id : room.residents) {
            if (!residents.insert(cat_id).second) {
                AddUnique(validation.errors, "duplicate-room-resident");
            }
        }
    }
    for (const auto& policy : input.protections) {
        if (policy.fixed_room && !room_ids.contains(*policy.fixed_room)) {
            AddUnique(validation.errors, "fixed-protection-room-unknown");
        }
    }

    std::unordered_set<snapshot::RoomId> capability_ids;
    for (const auto& capability : input.room_capabilities) {
        if (!room_ids.contains(capability.room_id)) {
            AddUnique(validation.errors, "capability-unknown-room");
        }
        if (!capability_ids.insert(capability.room_id).second) {
            AddUnique(validation.errors, "capability-duplicate-room");
        }
        if (capability.confirmed_hard_capacity.has_value() &&
            *capability.confirmed_hard_capacity == 0) {
            AddUnique(validation.limitations, "confirmed-zero-capacity-room");
        }
    }
    if (capability_ids.size() != room_ids.size()) {
        AddUnique(validation.errors, "capability-missing-room");
    }

    for (const auto candidate :
         input.classification.capacity_relief_candidates) {
        if (!cat_ids.contains(candidate)) {
            AddUnique(validation.errors, "relief-candidate-unknown-cat");
        }
    }
    std::unordered_set<snapshot::CatId> relief_ids;
    if (std::ranges::any_of(
            input.classification.capacity_relief_candidates,
            [&](const auto id) { return !relief_ids.insert(id).second; })) {
        AddUnique(validation.errors, "relief-candidate-duplicate-cat");
    }

    if (!house.capabilities.read_relationships) {
        AddUnique(validation.limitations, "relationships-unknown");
    }
    if (!house.capabilities.read_room_capacities) {
        AddUnique(validation.limitations, "room-capacities-unknown");
    }
    if (!house.capabilities.read_room_assignments) {
        AddUnique(validation.limitations, "room-assignments-unknown");
    }

    std::sort(validation.errors.begin(), validation.errors.end());
    std::sort(validation.limitations.begin(), validation.limitations.end());
    return validation;
}

}  // namespace autocattery::room_planning
