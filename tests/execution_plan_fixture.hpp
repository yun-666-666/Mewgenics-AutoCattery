#pragma once

#include "auto_cattery/protection/policy.hpp"
#include "execution_test_fakes.hpp"

namespace autocattery::tests::execution_test {

struct SealedFixture {
    snapshot::HouseSnapshot snapshot;
    classification::ClassificationPlan classification;
    std::vector<protection::ProtectionDecision> protections;
    room_planning::RoomPlan room_plan;
    execution::ApprovalResult approval;

    SealedFixture(
        std::size_t move_count = 3,
        std::size_t cull_count = 2) {
        snapshot.snapshot_id = 44;
        snapshot.scene_generation = 55;
        snapshot.game_day = 6;
        snapshot.capabilities.stable_cat_id = true;
        snapshot.capabilities.read_room_assignments = true;
        snapshot.rooms = {{"a", {}}, {"b", {}}};
        classification.source_snapshot_id = 44;
        classification.algorithm_version =
            classification::kClassificationAlgorithmVersion;
        room_plan.source_snapshot_id = 44;
        room_plan.disposition =
            room_planning::PlanDisposition::Complete;
        room_plan.fully_satisfied = true;

        const std::size_t total = move_count + cull_count;
        for (std::size_t index = 0; index < total; ++index) {
            AddCat(index, move_count);
        }
        room_plan.minimum_capacity_relief_required = cull_count;
        Seal(cull_count != 0);
    }

    FakeRead Reader() const {
        FakeRead read;
        const auto& precondition = approval.plan->Precondition();
        read.observation = {
            .scene_generation = precondition.scene_generation,
            .game_day = precondition.game_day,
            .game_build_identity =
                precondition.game_build_identity,
            .save_identity = precondition.save_identity,
            .snapshot_content_digest =
                precondition.snapshot_content_digest,
            .classification_digest =
                precondition.classification_digest,
            .plan_digest = precondition.plan_digest,
            .protection_digest =
                precondition.protection_digest,
            .cat_count = snapshot.cats.size()
        };
        return read;
    }

private:
    void AddCat(std::size_t index, std::size_t move_count) {
        snapshot::CatSnapshot cat;
        cat.id = static_cast<snapshot::CatId>(index + 1);
        cat.room_id = "a";
        cat.life_stage = snapshot::LifeStage::Adult;
        snapshot.cats.push_back(cat);
        snapshot.rooms.front().residents.push_back(cat.id);

        protection::ProtectionInput input;
        input.cat_id = cat.id;
        input.native.locked = snapshot::TriState::No;
        input.native.favorite = snapshot::TriState::No;
        input.native.special_state_present = snapshot::TriState::No;
        input.stable_identity_confirmed = true;
        protections.push_back(protection::Evaluate(input));
        classification.decisions.push_back({
            .cat_id = cat.id,
            .primary_role =
                classification::CatRole::GeneralReserve,
            .protection_level =
                protections.back().effective_level,
            .move_allowed = protections.back().move_allowed
        });

        if (index < move_count) {
            room_plan.moves.push_back({
                .cat_id = cat.id,
                .from_room = "a",
                .to_room = "b",
                .priority = static_cast<int>(index),
                .executable = false
            });
            return;
        }
        const auto order = index - move_count;
        classification.capacity_relief_candidates.push_back(cat.id);
        room_plan.capacity_relief_suggestions.push_back({
            .cat_id = cat.id,
            .candidate_order = order,
            .executable = false
        });
    }

    void Seal(bool approve_culls) {
        const auto digest = protection::BuildDigest(protections);
        approval = execution::PreconditionValidator{}.Validate({
            .operation_id = "transaction",
            .snapshot = snapshot,
            .classification = classification,
            .protections = protections,
            .room_plan = room_plan,
            .preview_protection_digest = digest,
            .current_protection_digest = digest,
            .expected_scene_generation = 55,
            .current_scene_generation = 55,
            .current_game_day = 6,
            .save_identity = "save",
            .game_build_identity = "build",
            .approve_culls = approve_culls
        });
    }
};

inline execution::ExecutionResult Execute(
    SealedFixture& fixture,
    FakeWrite& write,
    FakeRead& read,
    FakeBackup& backup,
    FakeRecovery& recovery,
    FakeJournal& journal) {
    execution::TransactionExecutor executor(
        write, read, backup, recovery, journal);
    return executor.Execute({
        *fixture.approval.plan,
        *fixture.approval.authorization,
        "fixture.sav",
        true
    });
}

}  // namespace autocattery::tests::execution_test
