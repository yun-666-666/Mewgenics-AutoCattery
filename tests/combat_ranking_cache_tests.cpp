#include "auto_cattery/scoring/combat_ranking_cache.hpp"

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

snapshot::HouseSnapshot CacheHouse() {
    snapshot::HouseSnapshot house;
    house.snapshot_id = 100;
    house.rooms.push_back({.id = "Floor1_Large", .residents = {1}});
    snapshot::CatSnapshot cat;
    cat.id = 1;
    cat.room_id = "Floor1_Large";
    cat.raw_ability_slots = {
        "DefaultMove", "BasicAttack", "", "", "", "", "", "", "", ""
    };
    cat.life_stage = snapshot::LifeStage::Adult;
    cat.available_for_combat = snapshot::TriState::Yes;
    cat.injured = snapshot::TriState::No;
    for (std::size_t index = 0; index < snapshot::kStatCount; ++index) {
        cat.genetic_stats.values[index] = 5;
        cat.heredity_bonus.values[index] = 0;
        cat.equipment_bonus.values[index] = 0;
    }
    house.cats.push_back(std::move(cat));
    return house;
}

}  // namespace

void RunCombatRankingCacheTests() {
    scoring::CombatScoringConfig first;
    auto reordered = first;
    first.active_ability_overrides.emplace("B", 2.0);
    first.active_ability_overrides.emplace("A", 1.0);
    reordered.active_ability_overrides.emplace("A", 1.0);
    reordered.active_ability_overrides.emplace("B", 2.0);
    const auto first_hash = scoring::CombatConfigHash(first);
    const auto reordered_hash = scoring::CombatConfigHash(reordered);
    AC_CHECK(static_cast<bool>(first_hash));
    AC_CHECK(static_cast<bool>(reordered_hash));
    AC_CHECK(first_hash.value == reordered_hash.value);

    auto different_category = scoring::CombatScoringConfig{};
    different_category.passive_overrides.emplace("A", 1.0);
    different_category.passive_overrides.emplace("B", 2.0);
    const auto different_category_hash =
        scoring::CombatConfigHash(different_category);
    AC_CHECK(static_cast<bool>(different_category_hash));
    AC_CHECK(first_hash.value != different_category_hash.value);

    scoring::CombatRankingCache cache;
    const auto house = CacheHouse();
    const auto initial = cache.GetOrCompute(house, first);
    const auto hit = cache.GetOrCompute(house, reordered);
    AC_CHECK(static_cast<bool>(initial));
    AC_CHECK(static_cast<bool>(hit));
    AC_CHECK(cache.ComputationCount() == 1);

    auto changed = first;
    changed.minimum_score = 36.0;
    const auto miss = cache.GetOrCompute(house, changed);
    AC_CHECK(static_cast<bool>(miss));
    AC_CHECK(cache.ComputationCount() == 2);
    AC_CHECK(miss.value.recommended_cat_ids.empty());

    auto next_snapshot = house;
    next_snapshot.snapshot_id = 101;
    AC_CHECK(static_cast<bool>(cache.GetOrCompute(next_snapshot, changed)));
    AC_CHECK(cache.ComputationCount() == 3);

    cache.Clear();
    AC_CHECK(static_cast<bool>(cache.GetOrCompute(next_snapshot, changed)));
    AC_CHECK(cache.ComputationCount() == 4);
}

}  // namespace autocattery::tests
