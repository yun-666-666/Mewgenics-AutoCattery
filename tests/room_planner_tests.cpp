#include "auto_cattery/room_planning/planner.hpp"

#include <algorithm>
#include <chrono>

#include "auto_cattery/protection/policy.hpp"
#include "test_support.hpp"

namespace autocattery::tests {
namespace {

using room_planning::CapabilityState;
using room_planning::RoomCapability;
using room_planning::RoomRole;

struct PlannerFixture {
    snapshot::HouseSnapshot house;
    classification::ClassificationPlan classification;
    std::vector<protection::ProtectionDecision> protections;
    std::vector<RoomCapability> capabilities;

    PlannerFixture(std::size_t cat_count, std::size_t room_count) {
        house.snapshot_id = 9009;
        house.capabilities.stable_cat_id = true;
        house.capabilities.read_room_assignments = true;
        house.capabilities.read_room_capacities = true;
        house.capabilities.read_relationships = true;
        classification.source_snapshot_id = house.snapshot_id;
        for (std::size_t index = 0; index < room_count; ++index) {
            const auto id = "R" + std::to_string(index);
            house.rooms.push_back({.id = id});
            capabilities.push_back(SafeRoom(
                id,
                index == 1 ? RoomRole::CombatStaging : RoomRole::General,
                cat_count + 1));
        }
        for (std::size_t index = 0; index < cat_count; ++index) {
            snapshot::CatSnapshot cat;
            cat.id = static_cast<snapshot::CatId>(index + 1);
            cat.life_stage = snapshot::LifeStage::Adult;
            if (room_count != 0) {
                cat.room_id = "R0";
                house.rooms.front().residents.push_back(cat.id);
            }
            house.cats.push_back(cat);

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
        }
    }

    static RoomCapability SafeRoom(
        std::string id,
        RoomRole role,
        std::size_t hard_capacity) {
        return {
            .room_id = std::move(id),
            .confirmed_role = role,
            .confirmed_hard_capacity = hard_capacity,
            .special_room = CapabilityState::No,
            .player_locked = CapabilityState::No,
            .forced_residents_present = CapabilityState::No,
            .can_receive_residents = CapabilityState::Yes,
            .can_release_residents = CapabilityState::Yes
        };
    }

    room_planning::RoomPlan Plan(
        room_planning::RoomPlanningConfig config = {}) const {
        const auto digest = protection::BuildDigest(protections);
        return room_planning::PlanRooms({
            house,
            classification,
            protections,
            capabilities,
            digest,
            digest
        }, config);
    }
};

void Protect(
    PlannerFixture& fixture,
    std::size_t index,
    protection::ProtectionLevel level,
    bool fail_closed = false) {
    auto& policy = fixture.protections.at(index);
    policy.effective_level = level;
    policy.fail_closed = fail_closed;
    policy.automatically_managed =
        level != protection::ProtectionLevel::FullyUnmanaged;
    policy.move_allowed =
        level == protection::ProtectionLevel::None ||
        level == protection::ProtectionLevel::NoCull;
    policy.cull_allowed =
        level == protection::ProtectionLevel::None ||
        level == protection::ProtectionLevel::NoMove;
    if (fail_closed) {
        policy.move_allowed = false;
        policy.cull_allowed = false;
    }
    auto& decision = fixture.classification.decisions.at(index);
    decision.protection_level = policy.effective_level;
    decision.move_allowed = policy.move_allowed;
    if (level == protection::ProtectionLevel::FullyUnmanaged) {
        decision.primary_role =
            classification::CatRole::ProtectedUnmanaged;
    }
}

void MakeReliefCandidate(PlannerFixture& fixture, std::size_t index) {
    auto& decision = fixture.classification.decisions.at(index);
    decision.primary_role = classification::CatRole::CullCandidate;
    decision.preview_cull_candidate = true;
    decision.destructive_action_allowed = false;
    fixture.classification.capacity_relief_candidates.push_back(
        decision.cat_id);
}

bool Has(
    const std::vector<std::string>& values,
    const std::string& value) {
    return std::ranges::find(values, value) != values.end();
}

}  // namespace

void RunRoomPlannerTests() {
    const PlannerFixture zero_rooms(0, 0);
    const auto zero_plan = zero_rooms.Plan();
    AC_CHECK(zero_plan.fully_satisfied);
    AC_CHECK(zero_plan.moves.empty());

    PlannerFixture missing_rooms(1, 0);
    const auto missing_plan = missing_rooms.Plan();
    AC_CHECK(!missing_plan.fully_satisfied);
    AC_CHECK(missing_plan.moves.empty());
    AC_CHECK(missing_plan.unplaced_cats.size() == 1);

    PlannerFixture one_room(2, 1);
    one_room.capabilities.front().confirmed_hard_capacity = 2;
    const auto one_plan = one_room.Plan();
    AC_CHECK(one_plan.fully_satisfied);
    AC_CHECK(one_plan.moves.empty());

    PlannerFixture role_move(2, 2);
    role_move.classification.decisions.front().primary_role =
        classification::CatRole::CombatRecommended;
    role_move.capabilities.front().confirmed_role = RoomRole::Breeding;
    const auto role_plan = role_move.Plan();
    AC_CHECK(role_plan.moves.size() == 1);
    AC_CHECK(role_plan.moves.front().cat_id == 1);
    AC_CHECK(role_plan.moves.front().to_room == "R1");
    AC_CHECK(!role_plan.moves.front().executable);

    PlannerFixture preferred_staging(1, 2);
    preferred_staging.classification.decisions.front().primary_role =
        classification::CatRole::CombatRecommended;
    const auto preferred_plan = preferred_staging.Plan();
    AC_CHECK(preferred_plan.moves.size() == 1);
    AC_CHECK(preferred_plan.moves.front().to_room == "R1");

    room_planning::RoomPlanningConfig no_staging_preference;
    no_staging_preference.prefer_single_combat_staging_room = false;
    AC_CHECK(
        preferred_staging.Plan(no_staging_preference).moves.empty());

    auto applied = role_move;
    applied.house.rooms.front().residents.erase(
        applied.house.rooms.front().residents.begin());
    applied.house.rooms.back().residents.push_back(1);
    applied.house.cats.front().room_id = "R1";
    const auto idempotent = applied.Plan();
    AC_CHECK(idempotent.moves.empty());

    PlannerFixture unknown_capacity(1, 2);
    unknown_capacity.classification.decisions.front().primary_role =
        classification::CatRole::CombatRecommended;
    unknown_capacity.capabilities.front().confirmed_hard_capacity.reset();
    const auto unknown_plan = unknown_capacity.Plan();
    AC_CHECK(unknown_plan.moves.empty());
    AC_CHECK(Has(
        unknown_plan.limitations,
        "source-room-capability-unknown"));

    PlannerFixture unknown_target(1, 2);
    unknown_target.classification.decisions.front().primary_role =
        classification::CatRole::CombatRecommended;
    unknown_target.capabilities.front().confirmed_role = RoomRole::Breeding;
    unknown_target.capabilities.back().confirmed_hard_capacity.reset();
    const auto unknown_target_plan = unknown_target.Plan();
    AC_CHECK(unknown_target_plan.moves.empty());

    PlannerFixture hard_capacity(3, 2);
    hard_capacity.capabilities.front().confirmed_hard_capacity = 1;
    hard_capacity.capabilities.back().confirmed_role = RoomRole::General;
    hard_capacity.capabilities.back().confirmed_hard_capacity = 1;
    MakeReliefCandidate(hard_capacity, 2);
    const auto capacity_plan = hard_capacity.Plan();
    AC_CHECK(capacity_plan.moves.size() == 1);
    AC_CHECK(capacity_plan.minimum_capacity_relief_required == 1);
    AC_CHECK(capacity_plan.capacity_relief_suggestions.size() == 1);
    AC_CHECK(
        capacity_plan.capacity_relief_suggestions.front().cat_id == 3);
    AC_CHECK(
        !capacity_plan.capacity_relief_suggestions.front().executable);
    AC_CHECK(capacity_plan.unplaced_cats.size() == 1);

    PlannerFixture insufficient(4, 1);
    insufficient.capabilities.front().confirmed_hard_capacity = 1;
    MakeReliefCandidate(insufficient, 3);
    const auto insufficient_plan = insufficient.Plan();
    AC_CHECK(insufficient_plan.minimum_capacity_relief_required == 3);
    AC_CHECK(insufficient_plan.capacity_relief_suggestions.size() == 1);
    AC_CHECK(Has(
        insufficient_plan.limitations,
        "safe-capacity-relief-candidates-insufficient"));

    PlannerFixture no_move(1, 2);
    no_move.classification.decisions.front().primary_role =
        classification::CatRole::CombatRecommended;
    no_move.capabilities.front().confirmed_role = RoomRole::Breeding;
    Protect(no_move, 0, protection::ProtectionLevel::NoMove);
    AC_CHECK(no_move.Plan().moves.empty());

    PlannerFixture no_cull_or_move(2, 1);
    no_cull_or_move.capabilities.front().confirmed_hard_capacity = 1;
    MakeReliefCandidate(no_cull_or_move, 1);
    Protect(
        no_cull_or_move,
        1,
        protection::ProtectionLevel::NoCullOrMove);
    const auto no_cull_or_move_plan = no_cull_or_move.Plan();
    AC_CHECK(no_cull_or_move_plan.moves.empty());
    AC_CHECK(no_cull_or_move_plan.capacity_relief_suggestions.empty());

    PlannerFixture no_cull(2, 1);
    no_cull.capabilities.front().confirmed_hard_capacity = 1;
    MakeReliefCandidate(no_cull, 1);
    Protect(no_cull, 1, protection::ProtectionLevel::NoCull);
    const auto no_cull_plan = no_cull.Plan();
    AC_CHECK(no_cull_plan.capacity_relief_suggestions.empty());

    PlannerFixture all_protected(3, 2);
    for (std::size_t index = 0; index < 3; ++index) {
        Protect(
            all_protected,
            index,
            protection::ProtectionLevel::NoCullOrMove);
    }
    const auto all_protected_plan = all_protected.Plan();
    AC_CHECK(all_protected_plan.moves.empty());
    AC_CHECK(all_protected_plan.capacity_relief_suggestions.empty());

    PlannerFixture unmanaged(1, 2);
    unmanaged.classification.decisions.front().primary_role =
        classification::CatRole::CombatRecommended;
    unmanaged.capabilities.front().confirmed_role = RoomRole::Breeding;
    Protect(
        unmanaged,
        0,
        protection::ProtectionLevel::FullyUnmanaged);
    AC_CHECK(unmanaged.Plan().moves.empty());

    PlannerFixture fail_closed(1, 2);
    fail_closed.classification.decisions.front().primary_role =
        classification::CatRole::CombatRecommended;
    fail_closed.capabilities.front().confirmed_role = RoomRole::Breeding;
    Protect(
        fail_closed,
        0,
        protection::ProtectionLevel::NoCullOrMove,
        true);
    AC_CHECK(fail_closed.Plan().moves.empty());

    PlannerFixture adventure(1, 2);
    adventure.house.cats.front().in_adventure_box = true;
    adventure.classification.decisions.front().primary_role =
        classification::CatRole::CombatRecommended;
    adventure.capabilities.front().confirmed_role = RoomRole::Breeding;
    AC_CHECK(adventure.Plan().moves.empty());

    PlannerFixture breeding(1, 2);
    breeding.classification.decisions.front().primary_role =
        classification::CatRole::BreedingCore;
    breeding.capabilities.front().confirmed_role = RoomRole::General;
    breeding.capabilities.back().confirmed_role = RoomRole::Breeding;
    const auto breeding_plan = breeding.Plan();
    AC_CHECK(breeding_plan.moves.empty());
    AC_CHECK(Has(
        breeding_plan.limitations,
        "breeding-pair-evidence-unavailable"));

    PlannerFixture soft_only(2, 2);
    soft_only.classification.decisions.front().primary_role =
        classification::CatRole::CombatRecommended;
    soft_only.capabilities.front().confirmed_role = RoomRole::Breeding;
    soft_only.capabilities.back().confirmed_hard_capacity.reset();
    room_planning::RoomPlanningConfig soft_config;
    soft_config.default_soft_capacity = 100;
    const auto soft_plan = soft_only.Plan(soft_config);
    AC_CHECK(soft_plan.moves.empty());

    PlannerFixture changed(1, 1);
    auto preview = protection::BuildDigest(changed.protections);
    Protect(changed, 0, protection::ProtectionLevel::NoMove);
    const auto current = protection::BuildDigest(changed.protections);
    const auto changed_plan = room_planning::PlanRooms({
        changed.house,
        changed.classification,
        changed.protections,
        changed.capabilities,
        preview,
        current
    }, {});
    AC_CHECK(
        changed_plan.disposition ==
        room_planning::PlanDisposition::CancelAndRepreview);
    AC_CHECK(changed_plan.moves.empty());

    PlannerFixture deterministic(25, 5);
    deterministic.capabilities.front().confirmed_hard_capacity = 3;
    const auto first = deterministic.Plan();
    const auto second = deterministic.Plan();
    AC_CHECK(first == second);

    PlannerFixture large(1000, 100);
    for (auto& room : large.house.rooms) {
        room.residents.clear();
    }
    for (std::size_t index = 0; index < large.house.cats.size(); ++index) {
        const auto room = index % large.house.rooms.size();
        large.house.cats[index].room_id = large.house.rooms[room].id;
        large.house.rooms[room].residents.push_back(
            large.house.cats[index].id);
    }
    for (auto& capability : large.capabilities) {
        capability.confirmed_hard_capacity = 10;
    }
    const auto started = std::chrono::steady_clock::now();
    const auto large_first = large.Plan();
    const auto large_second = large.Plan();
    const auto elapsed = std::chrono::steady_clock::now() - started;
    AC_CHECK(large_first == large_second);
    AC_CHECK(large_first.moves.empty());
    AC_CHECK(elapsed < std::chrono::seconds(5));
    AC_CHECK(std::ranges::all_of(
        large.classification.decisions,
        [](const auto& decision) {
            return !decision.destructive_action_allowed;
        }));
    AC_CHECK(!large_first.move_execution_allowed);
    AC_CHECK(!large_first.cull_execution_allowed);
}

}  // namespace autocattery::tests
