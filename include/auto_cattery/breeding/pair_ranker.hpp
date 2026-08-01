#pragma once

#include "auto_cattery/breeding/domain.hpp"
#include "auto_cattery/error.hpp"

namespace autocattery::breeding {

struct PairRanking {
    BreedingStage stage{BreedingStage::Foundation};
    std::vector<BreedingPairScore> ranked;
};

[[nodiscard]] Result<PairRanking> RankBreedingPairs(
    const snapshot::HouseSnapshot& house,
    const BreedingScoringConfig& config = {});

}  // namespace autocattery::breeding
