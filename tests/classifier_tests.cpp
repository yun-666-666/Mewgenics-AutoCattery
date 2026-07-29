#include "auto_cattery/classification/classifier.hpp"

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
        facts.emplace(
            cat.id,
            classification::CullSafetyFacts{
                .protected_from_cull = snapshot::TriState::No,
                .special_state_present = snapshot::TriState::No
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
    }
    const auto protected_plan = RunPlan(inputs, protected_facts);
    AC_CHECK(static_cast<bool>(protected_plan));
    AC_CHECK(protected_plan.value.quality_cull_candidates.empty());
    AC_CHECK(std::ranges::all_of(
        protected_plan.value.decisions,
        [](const auto& decision) {
            return decision.primary_role ==
                classification::CatRole::ProtectedUnmanaged;
        }));

    auto small = SafeInputs(3);
    small.classification_config.minimum_combat_pool = 8;
    small.classification_config.minimum_breeding_pool = 8;
    small.classification_config.minimum_general_reserve = 4;
    const auto small_plan = RunPlan(small, ClearedFacts(small.house));
    AC_CHECK(static_cast<bool>(small_plan));
    AC_CHECK(small_plan.value.quality_cull_candidates.empty());
    AC_CHECK(small_plan.value.global_warnings.size() >= 2);

    auto low_confidence = SafeInputs(10);
    low_confidence.house.cats.back().genetic_stats.values[6].reset();
    low_confidence.breeding_config.minimum_known_stats = 0;
    low_confidence.combat_config.minimum_known_stats = 0;
    low_confidence.classification_config
        .never_cull_if_data_confidence_below = 0.90;
    const auto low_confidence_plan =
        RunPlan(low_confidence, ClearedFacts(low_confidence.house));
    AC_CHECK(static_cast<bool>(low_confidence_plan));
    AC_CHECK(std::ranges::find(
        low_confidence_plan.value.quality_cull_candidates,
        low_confidence.house.cats.back().id) ==
        low_confidence_plan.value.quality_cull_candidates.end());

    auto invalid = inputs;
    invalid.classification_config.never_cull_if_data_confidence_below = 1.1;
    const auto invalid_plan = RunPlan(invalid, ClearedFacts(invalid.house));
    AC_CHECK(!static_cast<bool>(invalid_plan));
    AC_CHECK(invalid_plan.code == ErrorCode::ConfigInvalid);
}

}  // namespace autocattery::tests
