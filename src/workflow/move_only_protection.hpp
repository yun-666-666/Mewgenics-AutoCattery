#pragma once

#include <filesystem>

#include "auto_cattery/classification/domain.hpp"
#include "auto_cattery/protection/domain.hpp"

namespace autocattery::workflow::detail {

struct MoveOnlyProtectionSet {
    classification::CullSafetyFactsByCat safety;
    std::vector<protection::ProtectionDecision> decisions;
    protection::ProtectionDigest digest;
};

MoveOnlyProtectionSet BuildMoveOnlyProtections(
    const snapshot::HouseSnapshot& snapshot,
    const std::filesystem::path& sidecar_path);

}  // namespace autocattery::workflow::detail
