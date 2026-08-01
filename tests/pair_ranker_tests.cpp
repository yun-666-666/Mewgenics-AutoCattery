#include "auto_cattery/breeding/pair_ranker.hpp"

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

snapshot::CatSnapshot Adult(
    snapshot::CatId id,
    snapshot::CatSex sex,
    snapshot::CatSexuality sexuality,
    double coefficient,
    std::int32_t stat) {
    snapshot::CatSnapshot cat;
    cat.id = id;
    cat.sex = sex;
    cat.sexuality = sexuality;
    cat.sexuality_coefficient = coefficient;
    cat.life_stage = snapshot::LifeStage::Adult;
    cat.available_for_breeding = snapshot::TriState::Yes;
    for (auto& value : cat.genetic_stats.values) {
        value = stat;
    }
    return cat;
}

snapshot::HouseSnapshot PairHouse() {
    snapshot::HouseSnapshot house;
    house.snapshot_id = 900;
    house.capabilities.stable_cat_id = true;
    house.capabilities.read_genetic_stats = true;
    house.capabilities.read_sexuality = true;
    house.capabilities.read_relationships = true;
    house.cats = {
        Adult(1, snapshot::CatSex::Female,
              snapshot::CatSexuality::Straight, 0.0, 7),
        Adult(2, snapshot::CatSex::Male,
              snapshot::CatSexuality::Straight, 0.0, 7),
        Adult(3, snapshot::CatSex::Male,
              snapshot::CatSexuality::Bisexual, 0.5, 6)
    };
    house.pedigree_pair_coefficients = {
        {1, 2, 0.0}, {1, 3, 0.2}, {2, 3, 0.1}
    };
    return house;
}

}  // namespace

void RunPairRankerTests() {
    const auto ranked = breeding::RankBreedingPairs(PairHouse());
    AC_CHECK(static_cast<bool>(ranked));
    AC_CHECK(ranked.value.ranked.size() == 3);
    AC_CHECK(
        ranked.value.stage == breeding::BreedingStage::StableAllSeven);
    AC_CHECK(ranked.value.ranked[0].cat_a_id == 1);
    AC_CHECK(ranked.value.ranked[0].cat_b_id == 2);
    AC_CHECK(ranked.value.ranked[0].covered_seven_stats == 7);
    AC_CHECK(ranked.value.ranked[0].jointly_stable_seven_stats == 7);

    auto base_only = PairHouse();
    for (auto& value : base_only.cats[1].genetic_stats.values) {
        value = 6;
    }
    const auto base = breeding::RankBreedingPairs(base_only);
    AC_CHECK(static_cast<bool>(base));
    AC_CHECK(base.value.stage == breeding::BreedingStage::BaseAllSeven);

    auto hidden = PairHouse();
    hidden.capabilities.read_sexuality = false;
    const auto unavailable = breeding::RankBreedingPairs(hidden);
    AC_CHECK(!static_cast<bool>(unavailable));
    AC_CHECK(unavailable.code == ErrorCode::CatDataUnavailable);
}

}  // namespace autocattery::tests
