#pragma once

#include "auto_cattery/error.hpp"
#include "auto_cattery/scoring/domain.hpp"

namespace autocattery::scoring {

Result<void> Validate(const CombatScoringConfig& config);

Result<CombatScoreResult> ScoreCombatCat(
    const snapshot::CatSnapshot& cat,
    const CombatScoringConfig& config);

}  // namespace autocattery::scoring
