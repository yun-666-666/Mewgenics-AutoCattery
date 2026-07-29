#pragma once

#include <cstddef>
#include <vector>

#include "auto_cattery/recommendation/candidate_source.hpp"
#include "auto_cattery/scoring/domain.hpp"

namespace autocattery::recommendation {

struct InstantRecommendationResult {
    scoring::CombatRanking ranking;
    std::size_t candidate_count{};
};

class InstantRankingProvider final {
public:
    InstantRankingProvider(
        ICurrentCombatCandidateSource& source,
        scoring::CombatScoringConfig config);

    Result<InstantRecommendationResult> Recompute(
        const ui::UiContextSnapshot& context);

    [[nodiscard]] std::size_t ComputationCount() const noexcept;

private:
    ICurrentCombatCandidateSource& source_;
    scoring::CombatScoringConfig config_;
    std::size_t computation_count_{};
};

}  // namespace autocattery::recommendation
