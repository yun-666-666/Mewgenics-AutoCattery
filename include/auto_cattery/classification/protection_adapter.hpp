#pragma once

#include <string>
#include <unordered_map>

#include "auto_cattery/classification/domain.hpp"
#include "auto_cattery/protection/policy.hpp"
#include "auto_cattery/protection/sidecar.hpp"

namespace autocattery::classification {

using NativeProtectionFactsByCat = std::unordered_map<
    snapshot::CatId,
    protection::NativeProtectionFacts>;
using IdentityTokenByCat =
    std::unordered_map<snapshot::CatId, std::string>;

[[nodiscard]] CullSafetyFactsByCat BuildCullSafetyFacts(
    const snapshot::HouseSnapshot& snapshot,
    const NativeProtectionFactsByCat& native_facts,
    const protection::ProtectionSidecar& sidecar,
    const IdentityTokenByCat& identity_tokens);

}  // namespace autocattery::classification
