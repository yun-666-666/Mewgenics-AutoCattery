#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "auto_cattery/error.hpp"

namespace autocattery::snapshot::detail {

Result<std::vector<std::uint8_t>> DecodeCatStorageBlob(
    std::span<const std::uint8_t> stored);

}  // namespace autocattery::snapshot::detail
