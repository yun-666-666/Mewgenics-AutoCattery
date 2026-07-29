#include "auto_cattery/scoring/combat_ranker.hpp"

#include <chrono>

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

snapshot::CatSnapshot RankedCat(snapshot::CatId id, std::int32_t stat) {
    snapshot::CatSnapshot cat;
    cat.id = id;
    for (std::size_t index = 0; index < snapshot::kStatCount; ++index) {
        cat.genetic_stats.values[index] = stat;
        cat.heredity_bonus.values[index] = 0;
        cat.equipment_bonus.values[index] = 0;
    }
    cat.raw_ability_slots = {
        "DefaultMove", "BasicAttack", "", "", "", "", "", "", "", ""
    };
    cat.life_stage = snapshot::LifeStage::Adult;
    cat.available_for_combat = snapshot::TriState::Yes;
    cat.injured = snapshot::TriState::No;
    return cat;
}

snapshot::HouseSnapshot RankingHouse(std::size_t count) {
    snapshot::HouseSnapshot house;
    house.snapshot_id = 77;
    house.game_day = 18;
    house.capabilities.stable_cat_id = true;
    house.cats.reserve(count);
    house.rooms.push_back({.id = "Floor1_Large"});
    for (std::size_t index = 0; index < count; ++index) {
        const auto id = static_cast<snapshot::CatId>(count - index);
        house.cats.push_back(RankedCat(id, 5));
        house.cats.back().room_id = "Floor1_Large";
        house.rooms.front().residents.push_back(id);
    }
    return house;
}

bool SameRanking(
    const scoring::CombatRanking& left,
    const scoring::CombatRanking& right) {
    if (left.ranked.size() != right.ranked.size() ||
        left.recommended_cat_ids != right.recommended_cat_ids) {
        return false;
    }
    for (std::size_t index = 0; index < left.ranked.size(); ++index) {
        if (left.ranked[index].cat_id != right.ranked[index].cat_id ||
            left.ranked[index].score != right.ranked[index].score ||
            left.ranked[index].confidence != right.ranked[index].confidence) {
            return false;
        }
    }
    return true;
}

}  // namespace

void RunCombatRankerTests() {
    scoring::CombatScoringConfig config;
    config.recommended_count = 2;
    auto house = RankingHouse(3);
    const auto tied = scoring::RankCombatCats(house, config);
    AC_CHECK(static_cast<bool>(tied));
    AC_CHECK(tied.value.ranked.size() == 3);
    AC_CHECK(tied.value.ranked[0].cat_id == 1);
    AC_CHECK(tied.value.ranked[1].cat_id == 2);
    AC_CHECK(tied.value.ranked[2].cat_id == 3);
    AC_CHECK(tied.value.recommended_cat_ids.size() == 2);
    AC_CHECK(tied.value.recommended_cat_ids[0] == 1);
    AC_CHECK(tied.value.recommended_cat_ids[1] == 2);
    AC_CHECK(tied.value.source_snapshot_id == 77);
    AC_CHECK(tied.value.game_day_available);
    AC_CHECK(tied.value.game_day == 18);

    config.recommended_count = 10;
    config.minimum_score = 36.0;
    house.cats[0] = RankedCat(3, 6);
    house.cats[0].room_id = "Floor1_Large";
    const auto thresholded = scoring::RankCombatCats(house, config);
    AC_CHECK(static_cast<bool>(thresholded));
    AC_CHECK(thresholded.value.recommended_cat_ids.size() == 1);
    AC_CHECK(thresholded.value.recommended_cat_ids.front() == 3);

    const auto large_house = RankingHouse(1'000);
    const auto started = std::chrono::steady_clock::now();
    const auto baseline = scoring::RankCombatCats(large_house, config);
    const auto elapsed = std::chrono::steady_clock::now() - started;
    AC_CHECK(static_cast<bool>(baseline));
    AC_CHECK(baseline.value.ranked.size() == 1'000);
    AC_CHECK(elapsed < std::chrono::seconds(2));

    for (int repeat = 0; repeat < 100; ++repeat) {
        const auto rerun = scoring::RankCombatCats(large_house, config);
        AC_CHECK(static_cast<bool>(rerun));
        AC_CHECK(SameRanking(baseline.value, rerun.value));
    }

    auto invalid = RankingHouse(2);
    invalid.cats[1].id = invalid.cats[0].id;
    const auto rejected = scoring::RankCombatCats(invalid, config);
    AC_CHECK(!static_cast<bool>(rejected));
    AC_CHECK(rejected.code == ErrorCode::SnapshotInvalid);
}

}  // namespace autocattery::tests
