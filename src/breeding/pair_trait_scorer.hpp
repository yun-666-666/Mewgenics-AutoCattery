#pragma once

#include "auto_cattery/breeding/domain.hpp"

namespace autocattery::breeding::detail {

double StablePairTraitScore(
    const snapshot::HouseSnapshot& house,
    const snapshot::CatSnapshot& a,
    const snapshot::CatSnapshot& b,
    const BreedingScoringConfig& config);

}  // namespace autocattery::breeding::detail
