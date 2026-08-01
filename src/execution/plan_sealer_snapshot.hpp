#pragma once

#include <sstream>

#include "auto_cattery/snapshot/domain.hpp"

namespace autocattery::execution::detail {

void AppendSnapshotCapabilitiesAndBreeding(
    std::ostringstream& output,
    const snapshot::HouseSnapshot& snapshot);

}  // namespace autocattery::execution::detail
