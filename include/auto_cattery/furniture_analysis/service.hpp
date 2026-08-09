#pragma once

#include <cstdint>

#include "auto_cattery/furniture_analysis/domain.hpp"

namespace autocattery::furniture_analysis {

class FurnitureAnalysisService final {
public:
    explicit FurnitureAnalysisService(IFurnitureAnalysisSource& source);

    [[nodiscard]] Result<FurnitureAnalysisSnapshot> Analyze(
        std::uint64_t scene_generation);

private:
    IFurnitureAnalysisSource& source_;
};

}  // namespace autocattery::furniture_analysis
