#pragma once

#include <cstdint>
#include <span>

#include "auto_cattery/snapshot/domain.hpp"

namespace autocattery::snapshot::detail {

void ApplyUnlockedSexuality(
    std::span<const std::uint8_t> decoded_cat,
    std::size_t equipment_start,
    CatSnapshot& cat);

}  // namespace autocattery::snapshot::detail
