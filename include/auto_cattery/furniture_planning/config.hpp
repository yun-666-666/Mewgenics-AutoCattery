#pragma once

#include <cstddef>
#include <cstdint>

namespace autocattery::furniture_planning {

struct FurniturePurposeTargets {
    // These legacy member names and JSON keys are retained so v0.5.58 user
    // configs remain readable. Values are whole-room displayed attribute
    // targets and are never multiplied by the resident count.
    double comfort_per_resident{};
    double stimulation_per_resident{};
    double health_per_resident{};
    double mutation_per_resident{};
};

struct FurniturePlacementConfig {
    std::uint32_t version{1};
    FurniturePurposeTargets breeding{4.0, 4.0, 0.0, 0.0};
    FurniturePurposeTargets kitten_recovery{8.0, 0.0, 8.0, 0.0};
    // Combat comfort and stimulation are upper bounds. Health and mutation
    // remain lower bounds. Defaults create a controlled low-comfort room
    // without rewarding excess stimulation, while also targeting mutation.
    FurniturePurposeTargets combat{-8.0, 0.0, 0.0, 8.0};
    FurniturePurposeTargets mutation{-6.0, 0.0, 0.0, 8.0};
    FurniturePurposeTargets general{4.0, 4.0, 4.0, 4.0};
    std::size_t minimum_furnishing_coverage_percent{15};
    // Retained for v0.5.58+ configuration compatibility. Auto placement now
    // always continues filling after the room-purpose constraints are met;
    // the original false value contradicted the organizer's core contract.
    bool fill_remaining_capacity{true};
};

}  // namespace autocattery::furniture_planning
