#include "auto_cattery/breeding/pair_ranker.hpp"

#include <algorithm>

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
    auto low_libido_house = PairHouse();
    low_libido_house.cats[1].libido = snapshot::CatLibido::Low;
    const auto without_low = breeding::RankBreedingPairs(low_libido_house);
    AC_CHECK(without_low && without_low.value.ranked.front().cat_b_id == 3);
    AC_CHECK(std::ranges::none_of(without_low.value.ranked, [](const auto& pair) {
        return pair.eligible && (pair.cat_a_id == 2 || pair.cat_b_id == 2);
    }));
    const auto ranked = breeding::RankBreedingPairs(PairHouse());
    AC_CHECK(static_cast<bool>(ranked));
    AC_CHECK(ranked.value.ranked.size() == 3);
    AC_CHECK(
        ranked.value.stage == breeding::BreedingStage::StableAllSeven);
    AC_CHECK(ranked.value.ranked[0].cat_a_id == 1);
    AC_CHECK(ranked.value.ranked[0].cat_b_id == 2);
    AC_CHECK(ranked.value.ranked[0].covered_seven_stats == 7);
    AC_CHECK(ranked.value.ranked[0].jointly_stable_seven_stats == 7);
    const auto same_sex = std::ranges::find_if(
        ranked.value.ranked,
        [](const auto& pair) {
            return pair.cat_a_id == 2 && pair.cat_b_id == 3;
        });
    AC_CHECK(same_sex != ranked.value.ranked.end());
    AC_CHECK(!same_sex->eligible);
    AC_CHECK(std::ranges::find(
        same_sex->exclusion_reasons,
        "no-kitten-sex-pair") != same_sex->exclusion_reasons.end());

    auto base_only = PairHouse();
    for (auto& value : base_only.cats[1].genetic_stats.values) {
        value = 6;
    }
    const auto base = breeding::RankBreedingPairs(base_only);
    AC_CHECK(static_cast<bool>(base));
    AC_CHECK(base.value.stage == breeding::BreedingStage::BaseAllSeven);

    // Equal coverage, COI and orientation: configurable preferences must
    // influence pair selection as well as the individual-cat ranking.
    auto weighted_house = PairHouse();
    weighted_house.cats[0] = Adult(1, snapshot::CatSex::Female,
        snapshot::CatSexuality::Straight, 0.0, 4);
    weighted_house.cats[1] = Adult(2, snapshot::CatSex::Male,
        snapshot::CatSexuality::Straight, 0.0, 5);
    weighted_house.cats[2] = Adult(3, snapshot::CatSex::Male,
        snapshot::CatSexuality::Straight, 0.0, 5);
    weighted_house.cats[1].genetic_stats.values[0] = 6;
    weighted_house.cats[2].genetic_stats.values[3] = 6;
    weighted_house.pedigree_pair_coefficients = {{1, 2, 0.0}, {1, 3, 0.0}};
    const auto equal_weights = breeding::RankBreedingPairs(weighted_house);
    AC_CHECK(static_cast<bool>(equal_weights));
    AC_CHECK(equal_weights.value.ranked[0].cat_b_id == 2);
    breeding::BreedingScoringConfig preferred_stats;
    preferred_stats.stat_weights[3] = 2.0;
    const auto prefer_intelligence =
        breeding::RankBreedingPairs(weighted_house, preferred_stats);
    AC_CHECK(static_cast<bool>(prefer_intelligence));
    AC_CHECK(prefer_intelligence.value.ranked[0].cat_b_id == 3);
    preferred_stats.stat_weights[0] = 3.0;
    const auto prefer_strength =
        breeding::RankBreedingPairs(weighted_house, preferred_stats);
    AC_CHECK(static_cast<bool>(prefer_strength));
    AC_CHECK(prefer_strength.value.ranked[0].cat_b_id == 2);

    auto traits = PairHouse();
    auto favored = Adult(
        4, snapshot::CatSex::Male,
        snapshot::CatSexuality::Straight, 0.0, 7);
    favored.raw_ability_slots.resize(10);
    favored.raw_ability_slots[2] = "Gift";
    for (auto& cat : traits.cats) {
        cat.raw_ability_slots.resize(10);
    }
    traits.cats.push_back(std::move(favored));
    traits.capabilities.read_raw_ability_slots = true;
    traits.rooms.push_back({
        .id = "Stim",
        .attributes = snapshot::RoomAttributes{.stimulation = 32}
    });
    traits.pedigree_pair_coefficients.push_back({1, 4, 0.0});
    breeding::BreedingScoringConfig trait_config;
    trait_config.active_ability_overrides["Gift"] = 10.0;
    const auto trait_ranked =
        breeding::RankBreedingPairs(traits, trait_config);
    AC_CHECK(static_cast<bool>(trait_ranked));
    AC_CHECK(trait_ranked.value.ranked[0].cat_a_id == 1);
    AC_CHECK(trait_ranked.value.ranked[0].cat_b_id == 4);
    AC_CHECK(trait_ranked.value.ranked[0].trait_score > 0.0);

    traits.cats.back().raw_ability_slots.assign(10, std::string{});
    traits.cats.back().visual_traits = {{
        "body", "body", 300,
        snapshot::VisualTraitKind::Mutation
    }};
    traits.capabilities.read_visual_traits = true;
    trait_config.mutation_overrides["body:300"] = 20.0;
    const auto visual_ranked =
        breeding::RankBreedingPairs(traits, trait_config);
    AC_CHECK(static_cast<bool>(visual_ranked));
    AC_CHECK(visual_ranked.value.ranked[0].cat_b_id == 4);
    AC_CHECK(visual_ranked.value.ranked[0].trait_score > 0.0);

    // Assistance guarantees newborn stats even when the better trait donor
    // does not have all-seven parental stats. Native mode retains that gate.
    traits.cats.back().genetic_stats.values.fill(4);
    const auto native_donor = breeding::RankBreedingPairs(traits, trait_config);
    AC_CHECK(native_donor.value.ranked.front().cat_b_id == 2);
    const auto assisted_donor = breeding::RankBreedingPairs(traits, trait_config, true);
    AC_CHECK(assisted_donor.value.ranked.front().cat_b_id == 4);
    AC_CHECK(!assisted_donor.value.ranked.front().stable_all_seven);
    traits.cats.back().libido = snapshot::CatLibido::Low;
    AC_CHECK(breeding::RankBreedingPairs(traits, trait_config, true).value.ranked.front().cat_b_id == 2);

    traits.cats.back().libido = snapshot::CatLibido::Normal;
    traits.cats.back().genetic_stats.values.fill(7);
    traits.cats.back().visual_traits.clear();
    traits.cats.back().raw_ability_slots[6] = "DualWield";
    trait_config.passive_overrides["DualWield"] = 8;
    const auto passive_ranked = breeding::RankBreedingPairs(traits, trait_config);
    AC_CHECK(passive_ranked.value.ranked.front().cat_b_id == 4);
    traits.cats.back().raw_ability_slots[8] = "BadDisorder";
    trait_config.disorder_overrides["BadDisorder"] = 100;
    AC_CHECK(breeding::RankBreedingPairs(traits, trait_config).value.ranked.front().cat_b_id == 2);

    traits.cats.back().raw_ability_slots[8].clear();
    traits.cats[1].raw_ability_slots.assign(10, std::string{});
    traits.cats[1].raw_ability_slots[2] = "HugeSkill";
    traits.cats.back().raw_ability_slots[2] = "UsefulSkill";
    trait_config.active_ability_overrides["HugeSkill"] = 1000000;
    trait_config.active_ability_overrides["UsefulSkill"] = 8;
    snapshot::RoomSnapshot stimulating_room;
    stimulating_room.attributes = snapshot::RoomAttributes{.stimulation = 13};
    traits.rooms = {stimulating_room};
    // A huge score in one category must not erase useful passive donors.
    AC_CHECK(breeding::RankBreedingPairs(traits, trait_config).value.ranked.front().cat_b_id == 4);

    auto hidden = PairHouse();
    hidden.capabilities.read_sexuality = false;
    const auto unavailable = breeding::RankBreedingPairs(hidden);
    AC_CHECK(!static_cast<bool>(unavailable));
    AC_CHECK(unavailable.code == ErrorCode::CatDataUnavailable);
}

}  // namespace autocattery::tests
