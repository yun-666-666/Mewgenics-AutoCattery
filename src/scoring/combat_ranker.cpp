#include "auto_cattery/scoring/combat_ranker.hpp"

#include <algorithm>

#include "auto_cattery/scoring/combat_scorer.hpp"

namespace autocattery::scoring {

Result<CombatRanking> RankCombatCats(
    const snapshot::HouseSnapshot& snapshot,
    const CombatScoringConfig& config) {
    const auto config_validation = Validate(config);
    if (!config_validation) {
        return {
            {},
            config_validation.code,
            config_validation.message
        };
    }
    const auto snapshot_validation = snapshot::Validate(snapshot);
    if (!snapshot_validation.Valid()) {
        return {
            {},
            ErrorCode::SnapshotInvalid,
            "combat ranking requires a valid immutable snapshot"
        };
    }

    CombatRanking ranking;
    ranking.source_snapshot_id = snapshot.snapshot_id;
    ranking.algorithm_version = kCombatAlgorithmVersion;
    if (snapshot.game_day) {
        ranking.game_day = *snapshot.game_day;
        ranking.game_day_available = true;
    }

    ranking.ranked.reserve(snapshot.cats.size());
    for (const auto& cat : snapshot.cats) {
        auto result = ScoreCombatCat(cat, config);
        if (!result) {
            return {{}, result.code, result.message};
        }
        ranking.ranked.push_back(std::move(result.value));
    }

    std::sort(
        ranking.ranked.begin(),
        ranking.ranked.end(),
        [](const CombatScoreResult& left, const CombatScoreResult& right) {
            if (left.eligible != right.eligible) {
                return left.eligible > right.eligible;
            }
            if (left.score != right.score) {
                return left.score > right.score;
            }
            if (left.confidence != right.confidence) {
                return left.confidence > right.confidence;
            }
            if (left.total_stat_sum != right.total_stat_sum) {
                return left.total_stat_sum > right.total_stat_sum;
            }
            return left.cat_id < right.cat_id;
        });

    ranking.recommended_cat_ids.reserve(std::min(
        config.recommended_count,
        ranking.ranked.size()));
    for (const auto& result : ranking.ranked) {
        if (ranking.recommended_cat_ids.size() >= config.recommended_count) {
            break;
        }
        if (result.eligible && result.score >= config.minimum_score) {
            ranking.recommended_cat_ids.push_back(result.cat_id);
        }
    }
    return {std::move(ranking)};
}

}  // namespace autocattery::scoring
