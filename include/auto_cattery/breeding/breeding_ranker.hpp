#pragma once

#include "auto_cattery/breeding/domain.hpp"
#include "auto_cattery/error.hpp"

namespace autocattery::breeding {

Result<BreedingRanking> RankBreedingCats(
    const snapshot::HouseSnapshot& snapshot,
    const BreedingScoringConfig& config,
    bool avoid_inbreeding_pairs = false,
    bool offspring_all_seven_assist = false);

}  // namespace autocattery::breeding
