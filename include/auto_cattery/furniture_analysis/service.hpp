#pragma once

#include <cstdint>
#include <vector>

#include "auto_cattery/furniture_analysis/domain.hpp"

namespace autocattery::furniture_analysis {

class FurnitureAnalysisService final {
public:
    explicit FurnitureAnalysisService(IFurnitureAnalysisSource& source);

    [[nodiscard]] Result<FurnitureAnalysisSnapshot> Analyze(
        std::uint64_t scene_generation,
        const std::vector<snapshot::RoomId>& locked_room_ids = {},
        const std::vector<std::uint64_t>& blocked_warehouse_keys = {},
        bool allow_attribute_upgrades = true,
        const std::vector<room_planning::RoomPurposeAssignment>&
            room_purposes = {},
        const std::vector<furniture_planning::FurnitureLayoutStateEdge>&
            forbidden_layout_edges = {},
        const snapshot::RoomId& preferred_focus_room_id = {});

private:
    IFurnitureAnalysisSource& source_;
};

}  // namespace autocattery::furniture_analysis
