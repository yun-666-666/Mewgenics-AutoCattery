#pragma once

#include <array>
#include <cstdint>

namespace autocattery::breeding {

// Only newborn genetic stats change. Turning this off preserves native output.
inline bool ApplyOffspringAssistance(std::array<std::int32_t, 7>& stats, bool enabled) {
    if (!enabled || stats == std::array<std::int32_t, 7>{7, 7, 7, 7, 7, 7, 7}) return false;
    stats.fill(7);
    return true;
}

}  // namespace autocattery::breeding
