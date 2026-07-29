#include "auto_cattery/execution/precondition_validator.hpp"
#include "auto_cattery/execution/plan_sealer.hpp"

#include "auto_cattery/protection/policy.hpp"
#include "test_support.hpp"

namespace autocattery::tests {
namespace {

struct ApprovalFixture {
    snapshot::HouseSnapshot snapshot;
    classification::ClassificationPlan classification;
    std::vector<protection::ProtectionDecision> protections;
    room_planning::RoomPlan room_plan;

    ApprovalFixture() {
        snapshot.snapshot_id = 10;
        snapshot.scene_generation = 20;
        snapshot.game_day = 7;
        snapshot.capabilities.stable_cat_id = true;
        snapshot.capabilities.read_room_assignments = true;
        snapshot.rooms = {{"a", {1}}, {"b", {}}};
        snapshot::CatSnapshot cat;
        cat.id = 1;
        cat.room_id = "a";
        cat.life_stage = snapshot::LifeStage::Adult;
        snapshot.cats.push_back(cat);

        protection::ProtectionInput input;
        input.cat_id = 1;
        input.native.locked = snapshot::TriState::No;
        input.native.favorite = snapshot::TriState::No;
        input.native.special_state_present = snapshot::TriState::No;
        input.stable_identity_confirmed = true;
        protections.push_back(protection::Evaluate(input));

        classification.source_snapshot_id = snapshot.snapshot_id;
        classification.algorithm_version =
            classification::kClassificationAlgorithmVersion;
        classification.decisions.push_back({
            .cat_id = 1,
            .primary_role =
                classification::CatRole::GeneralReserve,
            .protection_level =
                protections.front().effective_level,
            .move_allowed = protections.front().move_allowed
        });

        room_plan.source_snapshot_id = snapshot.snapshot_id;
        room_plan.disposition =
            room_planning::PlanDisposition::Complete;
        room_plan.fully_satisfied = true;
        room_plan.moves.push_back({
            .cat_id = 1,
            .from_room = "a",
            .to_room = "b",
            .priority = 1,
            .executable = false
        });
    }

    execution::ApprovalResult Approve(bool culls = false) const {
        const auto digest = protection::BuildDigest(protections);
        return execution::PreconditionValidator{}.Validate({
            .operation_id = "operation-1",
            .snapshot = snapshot,
            .classification = classification,
            .protections = protections,
            .room_plan = room_plan,
            .preview_protection_digest = digest,
            .current_protection_digest = digest,
            .expected_scene_generation = 20,
            .current_scene_generation = 20,
            .current_game_day = 7,
            .save_identity = "save-content-digest",
            .game_build_identity = "build-sha256",
            .approve_culls = culls
        });
    }
};

}  // namespace

void RunExecutionPreconditionTests() {
    ApprovalFixture fixture;
    const auto approved = fixture.Approve();
    AC_CHECK(
        approved.disposition ==
        execution::ApprovalDisposition::Approved);
    AC_CHECK(approved.plan.has_value());
    AC_CHECK(approved.authorization.has_value());
    AC_CHECK(approved.plan->Moves().size() == 1);
    AC_CHECK(!fixture.room_plan.moves.front().executable);
    AC_CHECK(execution::AuthorizationMatches(
        *approved.plan, *approved.authorization));

    const auto first_digest =
        execution::DigestSnapshotContent(fixture.snapshot);
    fixture.snapshot.snapshot_id = 11;
    AC_CHECK(
        first_digest ==
        execution::DigestSnapshotContent(fixture.snapshot));

    ApprovalFixture changed_scene;
    changed_scene.snapshot.scene_generation = 21;
    AC_CHECK(
        changed_scene.Approve().disposition ==
        execution::ApprovalDisposition::CancelAndRepreview);

    ApprovalFixture unknown_room;
    unknown_room.room_plan.moves.front().to_room = "missing";
    AC_CHECK(
        unknown_room.Approve().disposition ==
        execution::ApprovalDisposition::CancelAndRepreview);

    ApprovalFixture protected_cat;
    protected_cat.protections.front().effective_level =
        protection::ProtectionLevel::NoMove;
    protected_cat.protections.front().move_allowed = false;
    AC_CHECK(
        protected_cat.Approve().disposition ==
        execution::ApprovalDisposition::CancelAndRepreview);

    ApprovalFixture no_cull_move;
    no_cull_move.protections.front().effective_level =
        protection::ProtectionLevel::NoCull;
    no_cull_move.protections.front().cull_allowed = false;
    no_cull_move.protections.front().move_allowed = true;
    AC_CHECK(
        no_cull_move.Approve().disposition ==
        execution::ApprovalDisposition::Approved);

    ApprovalFixture unmanaged;
    unmanaged.protections.front().effective_level =
        protection::ProtectionLevel::FullyUnmanaged;
    unmanaged.protections.front().automatically_managed = false;
    unmanaged.protections.front().move_allowed = false;
    unmanaged.protections.front().cull_allowed = false;
    AC_CHECK(
        unmanaged.Approve().disposition ==
        execution::ApprovalDisposition::CancelAndRepreview);

    ApprovalFixture fail_closed;
    fail_closed.protections.front().fail_closed = true;
    fail_closed.protections.front().move_allowed = false;
    fail_closed.protections.front().cull_allowed = false;
    AC_CHECK(
        fail_closed.Approve().disposition ==
        execution::ApprovalDisposition::CancelAndRepreview);

    ApprovalFixture adventure;
    adventure.snapshot.cats.front().in_adventure_box = true;
    AC_CHECK(
        adventure.Approve().disposition ==
        execution::ApprovalDisposition::CancelAndRepreview);

    ApprovalFixture no_identity;
    auto request_digest =
        protection::BuildDigest(no_identity.protections);
    const auto no_build =
        execution::PreconditionValidator{}.Validate({
            .operation_id = "operation-2",
            .snapshot = no_identity.snapshot,
            .classification = no_identity.classification,
            .protections = no_identity.protections,
            .room_plan = no_identity.room_plan,
            .preview_protection_digest = request_digest,
            .current_protection_digest = request_digest,
            .expected_scene_generation = 20,
            .current_scene_generation = 20,
            .current_game_day = 7,
            .save_identity = "save",
            .game_build_identity = ""
        });
    AC_CHECK(
        no_build.disposition ==
        execution::ApprovalDisposition::Unsupported);

    ApprovalFixture cull;
    cull.room_plan.moves.clear();
    cull.room_plan.minimum_capacity_relief_required = 1;
    cull.room_plan.capacity_relief_suggestions.push_back({
        .cat_id = 1,
        .candidate_order = 0,
        .executable = false
    });
    cull.classification.capacity_relief_candidates.push_back(1);
    const auto cull_approved = cull.Approve(true);
    AC_CHECK(cull_approved.plan->Culls().size() == 1);
    AC_CHECK(
        !cull.room_plan.capacity_relief_suggestions.front().executable);

    ApprovalFixture no_cull;
    no_cull.room_plan.moves.clear();
    no_cull.room_plan.minimum_capacity_relief_required = 1;
    no_cull.room_plan.capacity_relief_suggestions.push_back({
        .cat_id = 1,
        .candidate_order = 0
    });
    no_cull.classification.capacity_relief_candidates.push_back(1);
    no_cull.protections.front().effective_level =
        protection::ProtectionLevel::NoCull;
    no_cull.protections.front().cull_allowed = false;
    AC_CHECK(
        no_cull.Approve(true).disposition ==
        execution::ApprovalDisposition::CancelAndRepreview);

    ApprovalFixture no_move_cull;
    no_move_cull.room_plan.moves.clear();
    no_move_cull.room_plan.minimum_capacity_relief_required = 1;
    no_move_cull.room_plan.capacity_relief_suggestions.push_back({
        .cat_id = 1,
        .candidate_order = 0
    });
    no_move_cull.classification.capacity_relief_candidates.push_back(1);
    no_move_cull.protections.front().effective_level =
        protection::ProtectionLevel::NoMove;
    no_move_cull.protections.front().move_allowed = false;
    no_move_cull.protections.front().cull_allowed = true;
    AC_CHECK(
        no_move_cull.Approve(true).disposition ==
        execution::ApprovalDisposition::Approved);

    ApprovalFixture insufficient;
    insufficient.room_plan.moves.clear();
    insufficient.room_plan.minimum_capacity_relief_required = 1;
    AC_CHECK(
        insufficient.Approve(true).disposition ==
        execution::ApprovalDisposition::Unsupported);
}

}  // namespace autocattery::tests
