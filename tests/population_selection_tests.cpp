#include "auto_cattery/breeding/population_selection.hpp"
#include "test_support.hpp"
#include "auto_cattery/breeding/lineage_selection.hpp"
#include <algorithm>

int main() {
    using namespace autocattery;
    {
        snapshot::HouseSnapshot ancestry;
        for (int a = 1; a <= 6; ++a)
            for (int b = a + 1; b <= 6; ++b)
                ancestry.pedigree_pair_coefficients.push_back({a, b,
                    (a == 1 && b == 3) || (a == 2 && b == 4) ? 0.25 : 0.0});
        const std::vector<breeding::BreedingPairScore> pairs{
            {.cat_a_id=1, .cat_b_id=2, .eligible=true},
            {.cat_a_id=3, .cat_b_id=4, .eligible=true},
            {.cat_a_id=5, .cat_b_id=6, .eligible=true}};
        AC_CHECK((breeding::IndependentBreedingFamilies(pairs, ancestry) ==
            std::vector<snapshot::CatId>{1, 2, 5, 6}));
        ancestry.pedigree_pair_coefficients.clear();
        AC_CHECK(breeding::IndependentBreedingFamilies(pairs, ancestry).empty());
    }
    snapshot::HouseSnapshot house;
    house.capabilities.read_genetic_stats = true;
    house.capabilities.read_sexuality = true;
    house.capabilities.read_relationships = true;
    std::vector<protection::ProtectionCatOption> options;
    for (int id = 1; id <= 180; ++id) {
        snapshot::CatSnapshot cat;
        cat.id = id;
        cat.life_stage = snapshot::LifeStage::Adult;
        cat.genetic_stats.values.fill(id <= 30 ? 2 : 7);
        cat.available_for_combat = snapshot::TriState::Yes;
        cat.available_for_breeding = snapshot::TriState::Yes;
        cat.sex = id % 2 ? snapshot::CatSex::Male : snapshot::CatSex::Female;
        cat.sexuality = snapshot::CatSexuality::Straight;
        cat.sexuality_coefficient = 0;
        cat.libido = snapshot::CatLibido::Normal;
        cat.age_days = 10;
        house.cats.push_back(cat);
        options.push_back({.cat_id = id});
    }
    house.pedigree_pair_coefficients = {{1, 2, 0}, {3, 4, 0}};
    Config config;
    auto selected = breeding::SelectPopulation(house, options, config);
    AC_CHECK(static_cast<bool>(selected));
    AC_CHECK(selected.value.retained.size() == 150);
    AC_CHECK(selected.value.surplus.size() == 30);
    for (const auto id : {1, 2, 3, 4, 31, 32, 33, 34})
        AC_CHECK(std::ranges::find(selected.value.retained, id) != selected.value.retained.end());
    AC_CHECK(std::ranges::find(selected.value.surplus, 5) != selected.value.surplus.end());
    const auto first = selected.value.retained;
    std::ranges::reverse(house.cats);
    AC_CHECK(breeding::SelectPopulation(house, options, config).value.retained == first);
    config.room_planning.population_limit = 4;
    selected = breeding::SelectPopulation(house, options, config);
    AC_CHECK((selected.value.retained == std::vector<snapshot::CatId>{1, 2, 3, 4}));
    for (std::size_t i = 0; i < 5; ++i) options[i].level = protection::ProtectionLevel::NoCull;
    selected = breeding::SelectPopulation(house, options, config);
    AC_CHECK(selected.value.protected_count == 5 && !selected.value.limit_reached);
    AC_CHECK(selected.value.retained.size() == 5);
    config.room_planning.population_limit = 3;
    AC_CHECK(!breeding::SelectPopulation(house, options, config));
    return tests::failures ? 1 : 0;
}
