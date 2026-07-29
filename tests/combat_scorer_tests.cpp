#include "auto_cattery/scoring/combat_scorer.hpp"

#include <cmath>
#include <limits>

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

snapshot::CatSnapshot CompleteCat(snapshot::CatId id) {
    snapshot::CatSnapshot cat;
    cat.id = id;
    for (std::size_t index = 0; index < snapshot::kStatCount; ++index) {
        cat.genetic_stats.values[index] = static_cast<std::int32_t>(index + 1);
        cat.heredity_bonus.values[index] = 2;
        cat.equipment_bonus.values[index] = 3;
    }
    cat.raw_ability_slots = {
        "DefaultMove",
        "BasicAttack",
        "Fireball",
        "None",
        "",
        "Dash",
        "Tough",
        "",
        "Asthma",
        "None"
    };
    cat.life_stage = snapshot::LifeStage::Adult;
    cat.available_for_combat = snapshot::TriState::Yes;
    cat.injured = snapshot::TriState::No;
    return cat;
}

}  // namespace

void RunCombatScorerTests() {
    scoring::CombatScoringConfig config;
    const auto cat = CompleteCat(42);
    const auto scored = scoring::ScoreCombatCat(cat, config);
    AC_CHECK(static_cast<bool>(scored));
    AC_CHECK(scored.value.eligible);
    AC_CHECK(scored.value.score == 63.0);
    AC_CHECK(scored.value.total_stat_sum == 63);
    AC_CHECK(scored.value.confidence == 1.0);
    AC_CHECK(scored.value.components.size() == 11);
    AC_CHECK(scored.value.limitations.size() == 2);

    auto weighted = config;
    weighted.active_ability_overrides["Fireball"] = 4.5;
    weighted.passive_overrides["Tough"] = 2.0;
    weighted.disorder_overrides["Asthma"] = 3.0;
    const auto weighted_score = scoring::ScoreCombatCat(cat, weighted);
    AC_CHECK(static_cast<bool>(weighted_score));
    AC_CHECK(weighted_score.value.score == 66.5);

    auto missing_cat = cat;
    missing_cat.equipment_bonus.values[0].reset();
    const auto missing = scoring::ScoreCombatCat(missing_cat, config);
    AC_CHECK(static_cast<bool>(missing));
    AC_CHECK(!missing.value.eligible);
    AC_CHECK(missing.value.confidence == 6.0 / 7.0);
    AC_CHECK(missing.value.exclusion_reasons.size() == 1);

    auto partial_config = config;
    partial_config.minimum_known_stats = 6;
    partial_config.missing_stat_penalty = 0.25;
    const auto partial =
        scoring::ScoreCombatCat(missing_cat, partial_config);
    AC_CHECK(static_cast<bool>(partial));
    AC_CHECK(partial.value.eligible);
    AC_CHECK(partial.value.score == 56.75);

    auto negative = config;
    negative.stat_weights[0] = -1.0;
    const auto negative_score = scoring::ScoreCombatCat(cat, negative);
    AC_CHECK(static_cast<bool>(negative_score));
    AC_CHECK(negative_score.value.score == 51.0);

    auto invalid = config;
    invalid.minimum_score = std::numeric_limits<double>::quiet_NaN();
    const auto invalid_score = scoring::ScoreCombatCat(cat, invalid);
    AC_CHECK(!static_cast<bool>(invalid_score));
    AC_CHECK(invalid_score.code == ErrorCode::ConfigInvalid);

    invalid = config;
    invalid.passive_overrides["bad"] =
        std::numeric_limits<double>::infinity();
    AC_CHECK(!static_cast<bool>(scoring::Validate(invalid)));

    auto unavailable = cat;
    unavailable.available_for_combat = snapshot::TriState::No;
    const auto unavailable_score =
        scoring::ScoreCombatCat(unavailable, config);
    AC_CHECK(static_cast<bool>(unavailable_score));
    AC_CHECK(!unavailable_score.value.eligible);

    auto kitten = cat;
    kitten.life_stage = snapshot::LifeStage::Kitten;
    const auto kitten_score = scoring::ScoreCombatCat(kitten, config);
    AC_CHECK(static_cast<bool>(kitten_score));
    AC_CHECK(!kitten_score.value.eligible);

    auto injured = cat;
    injured.life_stage = snapshot::LifeStage::Adult;
    injured.available_for_combat = snapshot::TriState::Yes;
    injured.injured = snapshot::TriState::Yes;
    auto injury_config = config;
    injury_config.injury_penalty = 5.0;
    const auto injury_score =
        scoring::ScoreCombatCat(injured, injury_config);
    AC_CHECK(static_cast<bool>(injury_score));
    AC_CHECK(injury_score.value.eligible);
    AC_CHECK(injury_score.value.score == 58.0);

    injury_config.exclude_injured = true;
    const auto excluded_injury =
        scoring::ScoreCombatCat(injured, injury_config);
    AC_CHECK(static_cast<bool>(excluded_injury));
    AC_CHECK(!excluded_injury.value.eligible);

    auto unknown = cat;
    unknown.life_stage = snapshot::LifeStage::Unknown;
    unknown.available_for_combat = snapshot::TriState::Unknown;
    const auto guarded = scoring::ScoreCombatCat(unknown, config);
    AC_CHECK(static_cast<bool>(guarded));
    AC_CHECK(!guarded.value.eligible);

    auto permissive = config;
    permissive.require_confirmed_eligibility = false;
    const auto limited = scoring::ScoreCombatCat(unknown, permissive);
    AC_CHECK(static_cast<bool>(limited));
    AC_CHECK(limited.value.eligible);
    AC_CHECK(!limited.value.limitations.empty());
}

}  // namespace autocattery::tests
