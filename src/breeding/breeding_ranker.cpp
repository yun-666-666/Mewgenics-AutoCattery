#include "auto_cattery/breeding/breeding_ranker.hpp"

#include <algorithm>

#include "auto_cattery/breeding/breeding_scorer.hpp"
#include "auto_cattery/breeding/pair_ranker.hpp"
#include "pair_trait_scorer.hpp"

namespace autocattery::breeding {

Result<BreedingRanking> RankBreedingCats(
    const snapshot::HouseSnapshot& snapshot,
    const BreedingScoringConfig& config,
    bool avoid_inbreeding_pairs,
    bool offspring_all_seven_assist) {
    const auto config_validation = Validate(config);
    if (!config_validation) {
        return {{}, config_validation.code, config_validation.message};
    }
    if (!snapshot::Validate(snapshot).Valid()) {
        return {
            {},
            ErrorCode::SnapshotInvalid,
            "breeding ranking requires a valid immutable snapshot"
        };
    }

    BreedingRanking ranking;
    ranking.source_snapshot_id = snapshot.snapshot_id;
    ranking.algorithm_version = kBreedingAlgorithmVersion;
    if (snapshot.capabilities.read_genetic_stats &&
        snapshot.capabilities.read_sexuality &&
        snapshot.capabilities.read_relationships) {
        auto pairs = RankBreedingPairs(snapshot, config, offspring_all_seven_assist);
        if (!pairs) {
            return {{}, pairs.code, pairs.message};
        }
        ranking.stage = pairs.value.stage;
        ranking.ranked_pairs = std::move(pairs.value.ranked);
        if (avoid_inbreeding_pairs) {
            for (auto& pair : ranking.ranked_pairs) {
                if (pair.eligible &&
                    pair.offspring_inbreeding_coefficient.value_or(1.0) > 0.0) {
                    pair.eligible = false;
                    pair.exclusion_reasons.push_back("inbreeding-excluded-by-setting");
                }
            }
        }
        const auto recommended = std::ranges::find_if(
            ranking.ranked_pairs,
            [](const auto& pair) { return pair.eligible; });
        if (recommended != ranking.ranked_pairs.end()) {
            ranking.recommended_pair = *recommended;
        }
    }
    ranking.ranked.reserve(snapshot.cats.size());
    for (const auto& cat : snapshot.cats) {
        auto result = ScoreBreedingCat(cat, config, ranking.stage);
        if (!result) {
            return {{}, result.code, result.message};
        }
        result.value.trait_score = detail::StablePairTraitScore(snapshot, cat, cat, config);
        ranking.ranked.push_back(std::move(result.value));
    }

    std::sort(
        ranking.ranked.begin(),
        ranking.ranked.end(),
        [offspring_all_seven_assist](const auto& left, const auto& right) {
            if (left.eligible != right.eligible) {
                return left.eligible > right.eligible;
            }
            if (offspring_all_seven_assist && left.trait_score != right.trait_score) {
                return left.trait_score > right.trait_score;
            }
            const bool left_all_seven = left.heritable_stat_sum == 49;
            const bool right_all_seven = right.heritable_stat_sum == 49;
            if (left_all_seven != right_all_seven) return left_all_seven;
            if (left_all_seven && left.trait_score != right.trait_score) {
                return left.trait_score > right.trait_score;
            }
            if (left.score != right.score) {
                return left.score > right.score;
            }
            if (left.confidence != right.confidence) {
                return left.confidence > right.confidence;
            }
            if (left.heritable_stat_sum != right.heritable_stat_sum) {
                return left.heritable_stat_sum > right.heritable_stat_sum;
            }
            return left.cat_id < right.cat_id;
        });
    return {std::move(ranking)};
}

}  // namespace autocattery::breeding
