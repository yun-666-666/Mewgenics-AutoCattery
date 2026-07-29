#pragma once

#include "auto_cattery/breeding/domain.hpp"
#include "auto_cattery/error.hpp"

namespace autocattery::breeding {

Result<void> Validate(const BreedingScoringConfig& config);

Result<BreedingScoreResult> ScoreBreedingCat(
    const snapshot::CatSnapshot& cat,
    const BreedingScoringConfig& config);

}  // namespace autocattery::breeding
