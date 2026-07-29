#include "auto_cattery/breeding/breeding_ranker.hpp"

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

snapshot::HouseSnapshot BreedingHouse() {
    snapshot::HouseSnapshot house;
    house.snapshot_id = 701;
    house.rooms.push_back({.id = "Floor1"});
    for (snapshot::CatId id : {3, 1, 2}) {
        snapshot::CatSnapshot cat;
        cat.id = id;
        for (std::size_t index = 0; index < snapshot::kStatCount; ++index) {
            cat.genetic_stats.values[index] = 5;
            cat.heredity_bonus.values[index] = 0;
        }
        cat.raw_ability_slots.resize(10);
        cat.life_stage = snapshot::LifeStage::Adult;
        cat.available_for_breeding = snapshot::TriState::Yes;
        cat.room_id = "Floor1";
        house.rooms.front().residents.push_back(id);
        house.cats.push_back(std::move(cat));
    }
    return house;
}

}  // namespace

void RunBreedingRankerTests() {
    breeding::BreedingScoringConfig config;
    const auto ranked = breeding::RankBreedingCats(BreedingHouse(), config);
    AC_CHECK(static_cast<bool>(ranked));
    AC_CHECK(ranked.value.ranked.size() == 3);
    AC_CHECK(ranked.value.ranked[0].cat_id == 1);
    AC_CHECK(ranked.value.ranked[1].cat_id == 2);
    AC_CHECK(ranked.value.ranked[2].cat_id == 3);
    AC_CHECK(ranked.value.source_snapshot_id == 701);

    auto duplicate = BreedingHouse();
    duplicate.cats[1].id = duplicate.cats[0].id;
    const auto rejected = breeding::RankBreedingCats(duplicate, config);
    AC_CHECK(!static_cast<bool>(rejected));
    AC_CHECK(rejected.code == ErrorCode::SnapshotInvalid);
}

}  // namespace autocattery::tests
