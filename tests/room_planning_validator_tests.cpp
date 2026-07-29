#include "auto_cattery/room_planning/validator.hpp"

#include <algorithm>

#include "auto_cattery/protection/policy.hpp"
#include "test_support.hpp"

namespace autocattery::tests {
namespace {

struct ValidatorFixture {
    snapshot::HouseSnapshot house;
    classification::ClassificationPlan classification;
    std::vector<protection::ProtectionDecision> protections;
    std::vector<room_planning::RoomCapability> capabilities;

    ValidatorFixture() {
        house.snapshot_id = 9;
        house.capabilities.read_room_assignments = true;
        house.cats = {
            {.id = 1, .room_id = "A"},
            {.id = 2, .room_id = "B"}
        };
        house.rooms = {
            {.id = "A", .residents = {1}},
            {.id = "B", .residents = {2}}
        };
        classification.source_snapshot_id = house.snapshot_id;
        for (const auto& cat : house.cats) {
            protection::ProtectionInput policy_input;
            policy_input.cat_id = cat.id;
            policy_input.native.locked = snapshot::TriState::No;
            policy_input.native.favorite = snapshot::TriState::No;
            policy_input.native.special_state_present =
                snapshot::TriState::No;
            policy_input.stable_identity_confirmed = true;
            protections.push_back(protection::Evaluate(policy_input));
            classification.decisions.push_back({
                .cat_id = cat.id,
                .primary_role =
                    classification::CatRole::GeneralReserve,
                .protection_level =
                    protections.back().effective_level,
                .move_allowed = protections.back().move_allowed
            });
        }
        capabilities = {
            {.room_id = "A"},
            {.room_id = "B"}
        };
    }

    [[nodiscard]] room_planning::PlanningValidation Validate() const {
        const auto digest = protection::BuildDigest(protections);
        return room_planning::ValidateInput({
            house,
            classification,
            protections,
            capabilities,
            digest,
            digest
        });
    }
};

bool Has(
    const std::vector<std::string>& values,
    const std::string& value) {
    return std::ranges::find(values, value) != values.end();
}

}  // namespace

void RunRoomPlanningValidatorTests() {
    const ValidatorFixture valid;
    const auto valid_result = valid.Validate();
    AC_CHECK(valid_result.Valid());
    AC_CHECK(Has(valid_result.limitations, "relationships-unknown"));
    AC_CHECK(Has(valid_result.limitations, "room-capacities-unknown"));

    auto duplicate_room = ValidatorFixture{};
    duplicate_room.house.rooms.push_back(duplicate_room.house.rooms.front());
    AC_CHECK(!duplicate_room.Validate().Valid());

    auto duplicate_resident = ValidatorFixture{};
    duplicate_resident.house.rooms.front().residents.push_back(1);
    AC_CHECK(Has(
        duplicate_resident.Validate().errors,
        "duplicate-room-resident"));

    auto unknown_cat = ValidatorFixture{};
    unknown_cat.house.rooms.front().residents.push_back(99);
    AC_CHECK(!unknown_cat.Validate().Valid());

    auto multiple_rooms = ValidatorFixture{};
    multiple_rooms.house.rooms.back().residents.push_back(1);
    AC_CHECK(!multiple_rooms.Validate().Valid());

    auto classification_mismatch = ValidatorFixture{};
    classification_mismatch.classification.source_snapshot_id = 10;
    AC_CHECK(Has(
        classification_mismatch.Validate().errors,
        "classification-snapshot-mismatch"));

    auto missing_classification = ValidatorFixture{};
    missing_classification.classification.decisions.pop_back();
    AC_CHECK(Has(
        missing_classification.Validate().errors,
        "classification-missing-cat"));

    auto missing_protection = ValidatorFixture{};
    missing_protection.protections.pop_back();
    AC_CHECK(Has(
        missing_protection.Validate().errors,
        "protection-missing-cat"));

    auto policy_mismatch = ValidatorFixture{};
    policy_mismatch.classification.decisions.front().move_allowed = false;
    AC_CHECK(Has(
        policy_mismatch.Validate().errors,
        "protection-classification-mismatch"));

    auto missing_capability = ValidatorFixture{};
    missing_capability.capabilities.pop_back();
    AC_CHECK(Has(
        missing_capability.Validate().errors,
        "capability-missing-room"));

    auto unknown_relief = ValidatorFixture{};
    unknown_relief.classification.capacity_relief_candidates = {99};
    AC_CHECK(Has(
        unknown_relief.Validate().errors,
        "relief-candidate-unknown-cat"));
}

}  // namespace autocattery::tests
