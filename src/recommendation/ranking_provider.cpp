#include "auto_cattery/recommendation/ranking_provider.hpp"

#include <utility>

#include "auto_cattery/scoring/combat_ranker.hpp"

namespace autocattery::recommendation {

InstantRankingProvider::InstantRankingProvider(
    ICurrentCombatCandidateSource& source,
    scoring::CombatScoringConfig config)
    : source_(source),
      config_(std::move(config)) {}

Result<InstantRecommendationResult> InstantRankingProvider::Recompute(
    const ui::UiContextSnapshot& context) {
    if (context.kind != ui::UiContextKind::House ||
        !context.input_enabled || context.save_in_progress ||
        context.scene_generation == 0) {
        return {
            {},
            ErrorCode::SceneUnavailable,
            "instant combat ranking requires a safe House context"
        };
    }

    auto captured = source_.CaptureConfirmedCandidates(context);
    if (!captured) {
        return {{}, captured.code, captured.message};
    }
    if (!captured.value.capabilities.stable_cat_id ||
        captured.value.scene_generation != context.scene_generation) {
        return {
            {},
            ErrorCode::CatDataUnavailable,
            "current candidate snapshot lacks stable identity or generation"
        };
    }

    auto ranking = scoring::RankCombatCats(captured.value, config_);
    if (!ranking) {
        return {{}, ranking.code, ranking.message};
    }
    ++computation_count_;
    return {{
        .ranking = std::move(ranking.value),
        .candidate_count = captured.value.cats.size()
    }};
}

std::size_t InstantRankingProvider::ComputationCount() const noexcept {
    return computation_count_;
}

}  // namespace autocattery::recommendation
