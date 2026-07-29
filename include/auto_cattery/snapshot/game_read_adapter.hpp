#pragma once

#include <cstdint>

#include "auto_cattery/error.hpp"
#include "auto_cattery/snapshot/domain.hpp"

namespace autocattery::snapshot {

class IGameReadAdapter {
public:
    virtual ~IGameReadAdapter() = default;
    virtual Result<HouseSnapshot> CaptureHouseSnapshot(
        std::uint64_t scene_generation) = 0;
};

}  // namespace autocattery::snapshot
