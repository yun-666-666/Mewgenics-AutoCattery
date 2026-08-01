#include "auto_cattery/breeding/breeding_scorer.hpp"

#include <cmath>
#include <limits>

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

snapshot::CatSnapshot BreedingCat() {
    snapshot::CatSnapshot cat;
    cat.id = 41;
    for (std::size_t index = 0; index < snapshot::kStatCount; ++index) {
        cat.genetic_stats.values[index] = 1;
        cat.heredity_bonus.values[index] = 2;
        cat.equipment_bonus.values[index] = 100;
    }
    cat.raw_ability_slots = {
        "Move", "Attack", "Gift", "", "", "", "Calm", "",
        "Fragile", ""
    };
    cat.life_stage = snapshot::LifeStage::Adult;
    cat.available_for_breeding = snapshot::TriState::Yes;
    return cat;
}

}  // namespace

void RunBreedingScorerTests() {
    breeding::BreedingScoringConfig config;
    const auto baseline = breeding::ScoreBreedingCat(BreedingCat(), config);
    AC_CHECK(static_cast<bool>(baseline));
    AC_CHECK(baseline.value.eligible);
    AC_CHECK(baseline.value.score == 7.0);
    AC_CHECK(baseline.value.heritable_stat_sum == 7);
    AC_CHECK(baseline.value.confidence == 1.0);

    config.active_ability_overrides["Gift"] = 4.0;
    config.passive_overrides["Calm"] = 2.0;
    config.disorder_overrides["Fragile"] = 3.0;
    const auto configured =
        breeding::ScoreBreedingCat(BreedingCat(), config);
    AC_CHECK(static_cast<bool>(configured));
    AC_CHECK(configured.value.score == 10.0);

    auto unknown = BreedingCat();
    unknown.available_for_breeding = snapshot::TriState::Unknown;
    const auto excluded = breeding::ScoreBreedingCat(unknown, config);
    AC_CHECK(static_cast<bool>(excluded));
    AC_CHECK(!excluded.value.eligible);

    auto missing = BreedingCat();
    missing.genetic_stats.values[6].reset();
    const auto incomplete = breeding::ScoreBreedingCat(missing, config);
    AC_CHECK(static_cast<bool>(incomplete));
    AC_CHECK(!incomplete.value.eligible);
    AC_CHECK(incomplete.value.confidence == 6.0 / 7.0);

    config.minimum_score = 1'000.0;
    const auto thresholded =
        breeding::ScoreBreedingCat(BreedingCat(), config);
    AC_CHECK(static_cast<bool>(thresholded));
    AC_CHECK(!thresholded.value.eligible);

    config.minimum_score = std::numeric_limits<double>::infinity();
    const auto invalid = breeding::ScoreBreedingCat(BreedingCat(), config);
    AC_CHECK(!static_cast<bool>(invalid));
    AC_CHECK(invalid.code == ErrorCode::ConfigInvalid);
}

}  // namespace autocattery::tests
