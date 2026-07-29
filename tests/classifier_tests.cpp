#include "auto_cattery/classification/classifier.hpp"
#include "auto_cattery/classification/protection_adapter.hpp"

#include <algorithm>

#include "auto_cattery/breeding/breeding_ranker.hpp"
#include "auto_cattery/scoring/combat_ranker.hpp"
#include "test_support.hpp"

namespace autocattery::tests {
namespace {

snapshot::HouseSnapshot ClassificationHouse(std::size_t count) {
    snapshot::HouseSnapshot house;
    house.snapshot_id = 707;
    house.capabilities.stable_cat_id = true;
    house.capabilities.read_breeding_eligibility = true;
    house.capabilities.read_relationships = true;
    house.rooms.push_back({.id = "Floor1"});
    for (std::size_t index = 0; index < count; ++index) {
        snapshot::CatSnapshot cat;
        cat.id = static_cast<snapshot::CatId>(index + 1);
        const auto value = static_cast<std::int32_t>(count - index);
        for (std::size_t stat = 0; stat < snapshot::kStatCount; ++stat) {
            cat.genetic_stats.values[stat] = value;
            cat.heredity_bonus.values[stat] = 0;
            cat.equipment_bonus.values[stat] = 0;
        }
        cat.raw_ability_slots.resize(10);
        cat.life_stage = snapshot::LifeStage::Adult;
        cat.available_for_combat = snapshot::TriState::Yes;
        cat.available_for_breeding = snapshot::TriState::Yes;
        cat.injured = snapshot::TriState::No;
        cat.room_id = "Floor1";
        house.rooms.front().residents.push_back(cat.id);
        house.cats.push_back(std::move(cat));
    }
    return house;
}

classification::CullSafetyFactsByCat ClearedFacts(
    const snapshot::HouseSnapshot& house) {
    classification::CullSafetyFactsByCat facts;
    for (const auto& cat : house.cats) {
        protection::ProtectionInput input;
        input.cat_id = cat.id;
        input.native.locked = snapshot::TriState::No;
        input.native.favorite = snapshot::TriState::No;
        input.native.special_state_present = snapshot::TriState::No;
        input.stable_identity_confirmed = true;
        facts.emplace(
            cat.id,
            classification::CullSafetyFacts{
                .protected_from_cull = snapshot::TriState::No,
                .special_state_present = snapshot::TriState::No,
                .policy_decision = protection::Evaluate(input)
            });
    }
    return facts;
}

struct Inputs {
    snapshot::HouseSnapshot house;
    scoring::CombatScoringConfig combat_config;
    breeding::BreedingScoringConfig breeding_config;
    classification::ClassificationConfig classification_config;
};

Inputs SafeInputs(std::size_t count) {
    Inputs inputs;
    inputs.house = ClassificationHouse(count);
    inputs.combat_config.recommended_count = 2;
    inputs.breeding_config.core_breeders = 2;
    inputs.breeding_config.reserve_breeders = 2;
    inputs.classification_config.minimum_combat_pool = 2;
    inputs.classification_config.minimum_breeding_pool = 2;
    inputs.classification_config.minimum_general_reserve = 2;
    return inputs;
}

Result<classification::ClassificationPlan> RunPlan(
    const Inputs& inputs,
    const classification::CullSafetyFactsByCat& facts) {
    const auto combat =
        scoring::RankCombatCats(inputs.house, inputs.combat_config);
    const auto breeding =
        breeding::RankBreedingCats(inputs.house, inputs.breeding_config);
    if (!combat || !breeding) {
        return {
            {},
            ErrorCode::InternalError,
            "test ranking setup failed"
        };
    }
    return classification::ClassifyCats(
        inputs.house,
        combat.value,
        breeding.value,
        inputs.breeding_config,
        inputs.classification_config,
        facts);
}

}  // namespace

void RunClassifierTests() {
    const auto inputs = SafeInputs(10);
    const auto plan = RunPlan(inputs, ClearedFacts(inputs.house));
    AC_CHECK(static_cast<bool>(plan));
    AC_CHECK(plan.value.decisions.size() == 10);
    AC_CHECK(plan.value.quality_cull_candidates.size() == 4);
    AC_CHECK(plan.value.quality_cull_candidates.front() == 10);
    AC_CHECK(
        plan.value.capacity_relief_candidates ==
        plan.value.quality_cull_candidates);
    AC_CHECK(std::ranges::all_of(
        plan.value.decisions,
        [](const auto& decision) {
            return !decision.destructive_action_allowed;
        }));

    const auto unknown_guards =
        RunPlan(inputs, classification::CullSafetyFactsByCat{});
    AC_CHECK(static_cast<bool>(unknown_guards));
    AC_CHECK(unknown_guards.value.quality_cull_candidates.empty());

    auto protected_facts = ClearedFacts(inputs.house);
    for (auto& [_, facts] : protected_facts) {
        facts.protected_from_cull = snapshot::TriState::Yes;
        facts.policy_decision->effective_level =
            protection::ProtectionLevel::NoCull;
        facts.policy_decision->cull_allowed = false;
    }
    const auto protected_plan = RunPlan(inputs, protected_facts);
    AC_CHECK(static_cast<bool>(protected_plan));
    AC_CHECK(protected_plan.value.quality_cull_candidates.empty());

    auto level_facts = ClearedFacts(inputs.house);
    auto& no_cull = level_facts.at(10);
    no_cull.protected_from_cull = snapshot::TriState::Yes;
    no_cull.policy_decision->effective_level =
        protection::ProtectionLevel::NoCull;
    no_cull.policy_decision->cull_allowed = false;
    auto& no_move = level_facts.at(9);
    no_move.policy_decision->effective_level =
        protection::ProtectionLevel::NoMove;
    no_move.policy_decision->move_allowed = false;
    auto& unmanaged = level_facts.at(8);
    unmanaged.protected_from_cull = snapshot::TriState::Yes;
    unmanaged.policy_decision->effective_level =
        protection::ProtectionLevel::FullyUnmanaged;
    unmanaged.policy_decision->automatically_managed = false;
    unmanaged.policy_decision->cull_allowed = false;
    unmanaged.policy_decision->move_allowed = false;
    const auto level_plan = RunPlan(inputs, level_facts);
    AC_CHECK(static_cast<bool>(level_plan));
    AC_CHECK(std::ranges::find(
        level_plan.value.quality_cull_candidates, 10) ==
        level_plan.value.quality_cull_candidates.end());
    AC_CHECK(std::ranges::find(
        level_plan.value.quality_cull_candidates, 9) !=
        level_plan.value.quality_cull_candidates.end());
    const auto no_move_decision = std::ranges::find_if(
        level_plan.value.decisions,
        [](const auto& decision) { return decision.cat_id == 9; });
    AC_CHECK(no_move_decision != level_plan.value.decisions.end());
    AC_CHECK(!no_move_decision->move_allowed);
    const auto unmanaged_decision = std::ranges::find_if(
        level_plan.value.decisions,
        [](const auto& decision) { return decision.cat_id == 8; });
    AC_CHECK(unmanaged_decision != level_plan.value.decisions.end());
    AC_CHECK(
        unmanaged_decision->primary_role ==
        classification::CatRole::ProtectedUnmanaged);

    auto blacklist_facts = ClearedFacts(inputs.house);
    blacklist_facts.at(9).policy_decision->blacklist_preferred = true;
    blacklist_facts.at(10).policy_decision->blacklist_preferred = true;
    blacklist_facts.at(10).protected_from_cull = snapshot::TriState::Yes;
    blacklist_facts.at(10).policy_decision->effective_level =
        protection::ProtectionLevel::NoCull;
    blacklist_facts.at(10).policy_decision->cull_allowed = false;
    const auto blacklist_plan = RunPlan(inputs, blacklist_facts);
    AC_CHECK(static_cast<bool>(blacklist_plan));
    AC_CHECK(blacklist_plan.value.quality_cull_candidates.front() == 9);
    AC_CHECK(std::ranges::find(
        blacklist_plan.value.quality_cull_candidates, 10) ==
        blacklist_plan.value.quality_cull_candidates.end());

    auto small = SafeInputs(3);
    small.classification_config.minimum_combat_pool = 8;
    small.classification_config.minimum_breeding_pool = 8;
    small.classification_config.minimum_general_reserve = 4;
    const auto small_plan = RunPlan(small, ClearedFacts(small.house));
    AC_CHECK(static_cast<bool>(small_plan));
    AC_CHECK(small_plan.value.quality_cull_candidates.empty());
    AC_CHECK(small_plan.value.global_warnings.size() >= 2);

    auto small_blacklist = ClearedFacts(small.house);
    for (auto& [_, facts] : small_blacklist) {
        facts.policy_decision->blacklist_preferred = true;
    }
    const auto small_blacklist_plan =
        RunPlan(small, small_blacklist);
    AC_CHECK(static_cast<bool>(small_blacklist_plan));
    AC_CHECK(
        small_blacklist_plan.value.quality_cull_candidates.empty());

    auto low_confidence = SafeInputs(10);
    low_confidence.house.cats.back().genetic_stats.values[6].reset();
    low_confidence.breeding_config.minimum_known_stats = 0;
    low_confidence.combat_config.minimum_known_stats = 0;
    low_confidence.classification_config
        .never_cull_if_data_confidence_below = 0.90;
    auto low_confidence_facts = ClearedFacts(low_confidence.house);
    low_confidence_facts.at(
        low_confidence.house.cats.back().id)
        .policy_decision->blacklist_preferred = true;
    const auto low_confidence_plan =
        RunPlan(low_confidence, low_confidence_facts);
    AC_CHECK(static_cast<bool>(low_confidence_plan));
    AC_CHECK(std::ranges::find(
        low_confidence_plan.value.quality_cull_candidates,
        low_confidence.house.cats.back().id) ==
        low_confidence_plan.value.quality_cull_candidates.end());

    protection::ProtectionSidecar sidecar;
    sidecar.status = protection::SidecarLoadStatus::Loaded;
    sidecar.destructive_actions_blocked = false;
    sidecar.records.emplace(
        10,
        protection::SidecarRecord{
            protection::ProtectionRecord{
                10,
                protection::ProtectionLevel::NoCull,
                "whitelist",
                std::nullopt},
            "expected"});
    sidecar.blacklist.insert(10);
    classification::NativeProtectionFactsByCat native;
    for (const auto& cat : inputs.house.cats) {
        native.emplace(
            cat.id,
            protection::NativeProtectionFacts{
                snapshot::TriState::No,
                snapshot::TriState::No,
                snapshot::TriState::No});
    }
    classification::IdentityTokenByCat identities{{10, "expected"}};
    const auto adapted = classification::BuildCullSafetyFacts(
        inputs.house, native, sidecar, identities);
    AC_CHECK(!adapted.at(10).policy_decision->cull_allowed);
    AC_CHECK(adapted.at(10).policy_decision->blacklist_preferred);
    const auto adapted_plan = RunPlan(inputs, adapted);
    AC_CHECK(static_cast<bool>(adapted_plan));
    AC_CHECK(std::ranges::find(
        adapted_plan.value.quality_cull_candidates, 10) ==
        adapted_plan.value.quality_cull_candidates.end());

    identities.at(10) = "conflict";
    const auto conflicted = classification::BuildCullSafetyFacts(
        inputs.house, native, sidecar, identities);
    AC_CHECK(conflicted.at(10).policy_decision->fail_closed);
    AC_CHECK(!conflicted.at(10).policy_decision->cull_allowed);

    protection::ProtectionSidecar missing_sidecar;
    const auto missing_facts = classification::BuildCullSafetyFacts(
        inputs.house, native, missing_sidecar, identities);
    const auto missing_plan = RunPlan(inputs, missing_facts);
    AC_CHECK(static_cast<bool>(missing_plan));
    AC_CHECK(missing_plan.value.quality_cull_candidates.empty());

    auto invalid = inputs;
    invalid.classification_config.never_cull_if_data_confidence_below = 1.1;
    const auto invalid_plan = RunPlan(invalid, ClearedFacts(invalid.house));
    AC_CHECK(!static_cast<bool>(invalid_plan));
    AC_CHECK(invalid_plan.code == ErrorCode::ConfigInvalid);
}

}  // namespace autocattery::tests
