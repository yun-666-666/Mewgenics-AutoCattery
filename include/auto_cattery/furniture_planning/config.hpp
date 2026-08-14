#pragma once

#include <cstddef>
#include <cstdint>

namespace autocattery::furniture_planning {

struct FurniturePurposeTargets {
    double comfort_per_resident{};
    double stimulation_per_resident{};
    double health_per_resident{};
    double mutation_per_resident{};
};

struct FurniturePlacementConfig {
    std::uint32_t version{1};
    FurniturePurposeTargets breeding{2.0, 2.0, 0.0, 0.0};
    FurniturePurposeTargets kitten_recovery{2.0, 0.0, 2.0, 0.0};
    // Combat comfort and stimulation are upper bounds. Health and mutation
    // remain lower bounds. Defaults create a controlled low-comfort room
    // without rewarding excess stimulation, while also targeting mutation.
    FurniturePurposeTargets combat{-2.0, 0.0, 0.0, 2.0};
    FurniturePurposeTargets mutation{-2.0, 0.0, 0.0, 2.0};
    FurniturePurposeTargets general{1.0, 1.0, 1.0, 1.0};
    std::size_t minimum_furnishing_coverage_percent{15};
    bool fill_remaining_capacity{};
};

}  // namespace autocattery::furniture_planning
