#include "auto_cattery/breeding/pair_ranker.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <numbers>
#include <tuple>
#include <unordered_map>

#include "auto_cattery/breeding/breeding_scorer.hpp"
#include "pair_trait_scorer.hpp"

namespace autocattery::breeding {
namespace {

using PairKey = std::pair<snapshot::CatId, snapshot::CatId>;

PairKey Key(snapshot::CatId a, snapshot::CatId b) {
    return a < b ? PairKey{a, b} : PairKey{b, a};
}

bool IsAllSeven(const snapshot::CatSnapshot& cat) {
    return std::ranges::all_of(
        cat.genetic_stats.values,
        [](const auto& value) { return value && *value == 7; });
}

bool HardOrientationMismatch(
    const snapshot::CatSnapshot& a,
    const snapshot::CatSnapshot& b) {
    return a.sexuality == snapshot::CatSexuality::Gay &&
        b.sexuality == snapshot::CatSexuality::Gay;
}

bool IsKnownOppositeSexPair(
    const snapshot::CatSnapshot& a,
    const snapshot::CatSnapshot& b) {
    return
        (a.sex == snapshot::CatSex::Female &&
         b.sex == snapshot::CatSex::Male) ||
        (a.sex == snapshot::CatSex::Male &&
         b.sex == snapshot::CatSex::Female);
}

double OrientationQuality(
    const snapshot::CatSnapshot& a,
    const snapshot::CatSnapshot& b) {
    const bool same_sex = a.sex == b.sex;
    const auto quality = [same_sex](double coefficient) {
        const auto angle = std::numbers::pi * 0.5 * coefficient;
        return same_sex ? std::sin(angle) : std::cos(angle);
    };
    return (quality(*a.sexuality_coefficient) +
            quality(*b.sexuality_coefficient)) * 0.5;
}

BreedingPairScore ScorePair(
    const snapshot::CatSnapshot& a,
    const snapshot::CatSnapshot& b,
    const std::map<PairKey, double>& coefficients,
    const BreedingScoringConfig& config) {
    BreedingPairScore result{.cat_a_id = a.id, .cat_b_id = b.id};
    if (a.libido == snapshot::CatLibido::Low ||
        b.libido == snapshot::CatLibido::Low) {
        result.exclusion_reasons.push_back("low-libido-not-selected-for-breeding");
    }
    if (a.available_for_breeding != snapshot::TriState::Yes ||
        b.available_for_breeding != snapshot::TriState::Yes) {
        result.exclusion_reasons.push_back("not-adult-breeding-pair");
    }
    const bool known_opposite_sex = IsKnownOppositeSexPair(a, b);
    if (!known_opposite_sex) {
        result.exclusion_reasons.push_back("no-kitten-sex-pair");
    }
    if (!a.sexuality_coefficient || !b.sexuality_coefficient) {
        result.exclusion_reasons.push_back("sexuality-unavailable");
    } else if (known_opposite_sex && HardOrientationMismatch(a, b)) {
        result.exclusion_reasons.push_back("orientation-incompatible");
    }
    const auto coefficient = coefficients.find(Key(a.id, b.id));
    if (coefficient == coefficients.end()) {
        result.exclusion_reasons.push_back("pair-coi-unavailable");
    } else {
        result.offspring_inbreeding_coefficient = coefficient->second;
    }
    double coverage_sum{};
    for (std::size_t i = 0; i < snapshot::kStatCount; ++i) {
        const auto av = a.genetic_stats.values[i];
        const auto bv = b.genetic_stats.values[i];
        if (!av || !bv) {
            result.exclusion_reasons.push_back("base-stats-unavailable");
            break;
        }
        const auto best = std::max(*av, *bv);
        coverage_sum += static_cast<double>(best) * config.stat_weights[i];
        result.covered_seven_stats += best == 7 ? 1U : 0U;
        result.jointly_stable_seven_stats +=
            *av == 7 && *bv == 7 ? 1U : 0U;
    }
    result.eligible = result.exclusion_reasons.empty();
    if (result.eligible) {
        result.stable_all_seven =
            result.jointly_stable_seven_stats == snapshot::kStatCount &&
            result.offspring_inbreeding_coefficient == 0.0;
        result.score =
            static_cast<double>(result.covered_seven_stats) * 1000.0 +
            static_cast<double>(result.jointly_stable_seven_stats) * 100.0 +
            coverage_sum * 10.0 -
            *result.offspring_inbreeding_coefficient * 500.0 +
            OrientationQuality(a, b) * 10.0;
    }
    return result;
}

}  // namespace

Result<PairRanking> RankBreedingPairs(
    const snapshot::HouseSnapshot& house,
    const BreedingScoringConfig& config) {
    const auto config_validation = Validate(config);
    if (!config_validation) {
        return {{}, config_validation.code, config_validation.message};
    }
    if (!house.capabilities.read_genetic_stats ||
        !house.capabilities.read_sexuality ||
        !house.capabilities.read_relationships) {
        return {
            {}, ErrorCode::CatDataUnavailable,
            "breeding pair fields are not unlocked"
        };
    }
    std::map<PairKey, double> coefficients;
    for (const auto& value : house.pedigree_pair_coefficients) {
        coefficients[Key(value.cat_a_id, value.cat_b_id)] = value.coefficient;
    }
    PairRanking ranking;
    bool has_all_seven{};
    for (std::size_t i = 0; i < house.cats.size(); ++i) {
        has_all_seven = has_all_seven || IsAllSeven(house.cats[i]);
        for (std::size_t j = i + 1; j < house.cats.size(); ++j) {
            ranking.ranked.push_back(
                ScorePair(house.cats[i], house.cats[j], coefficients, config));
        }
    }
    const bool stable = std::ranges::any_of(
        ranking.ranked,
        [](const auto& pair) { return pair.stable_all_seven; });
    ranking.stage = stable
        ? BreedingStage::StableAllSeven
        : has_all_seven ? BreedingStage::BaseAllSeven
                        : BreedingStage::Foundation;
    if (stable) {
        std::unordered_map<snapshot::CatId, const snapshot::CatSnapshot*> cats;
        for (const auto& cat : house.cats) {
            cats.emplace(cat.id, &cat);
        }
        for (auto& pair : ranking.ranked) {
            if (!pair.stable_all_seven) {
                continue;
            }
            pair.trait_score = detail::StablePairTraitScore(
                house,
                *cats.at(pair.cat_a_id),
                *cats.at(pair.cat_b_id),
                config);
            pair.score += pair.trait_score;
        }
    }
    std::sort(
        ranking.ranked.begin(), ranking.ranked.end(),
        [stable](const auto& left, const auto& right) {
            return std::tuple{
                !left.eligible,
                stable && !left.stable_all_seven,
                stable ? -left.trait_score : 0.0,
                -left.score,
                left.cat_a_id,
                left.cat_b_id
            } < std::tuple{
                !right.eligible,
                stable && !right.stable_all_seven,
                stable ? -right.trait_score : 0.0,
                -right.score,
                right.cat_a_id,
                right.cat_b_id
            };
        });
    return {std::move(ranking)};
}

}  // namespace autocattery::breeding
