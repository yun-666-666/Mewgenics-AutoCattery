#pragma once

#include "auto_cattery/breeding/domain.hpp"
#include "auto_cattery/classification/domain.hpp"
#include "auto_cattery/error.hpp"
#include "auto_cattery/scoring/domain.hpp"

namespace autocattery::classification {

Result<void> Validate(const ClassificationConfig& config);

Result<ClassificationPlan> ClassifyCats(
    const snapshot::HouseSnapshot& snapshot,
    const scoring::CombatRanking& combat,
    const breeding::BreedingRanking& breeding,
    const breeding::BreedingScoringConfig& breeding_config,
    const ClassificationConfig& config,
    const CullSafetyFactsByCat& safety_facts);

}  // namespace autocattery::classification
