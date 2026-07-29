#pragma once

#include "auto_cattery/error.hpp"
#include "auto_cattery/scoring/domain.hpp"

namespace autocattery::scoring {

Result<CombatRanking> RankCombatCats(
    const snapshot::HouseSnapshot& snapshot,
    const CombatScoringConfig& config);

}  // namespace autocattery::scoring
