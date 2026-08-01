#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "auto_cattery/error.hpp"
#include "auto_cattery/snapshot/domain.hpp"

namespace autocattery::snapshot::detail {

struct PedigreeData {
    std::vector<PedigreeEntry> entries;
    std::vector<PedigreePairCoefficient> pair_coefficients;
};

[[nodiscard]] Result<PedigreeData> ParsePedigreeBlob(
    std::span<const std::byte> blob);

}  // namespace autocattery::snapshot::detail
