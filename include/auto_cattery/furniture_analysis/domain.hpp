#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "auto_cattery/error.hpp"
#include "auto_cattery/furniture_planning/layout_solver.hpp"
#include "auto_cattery/snapshot/detail/furniture_attributes.hpp"
#include "auto_cattery/snapshot/detail/furniture_geometry.hpp"
#include "auto_cattery/snapshot/domain.hpp"

namespace autocattery::furniture_analysis {

struct FurnitureAnalysisSourceSnapshot {
    snapshot::HouseSnapshot house;
    std::vector<snapshot::detail::FurniturePlacement> furniture;
    snapshot::detail::HouseGeometryCatalog geometry;
    snapshot::detail::FurnitureInfoCatalog furniture_info;
    snapshot::detail::FurnitureCatalog furniture_effects;
    std::size_t available_room_count{};
    std::vector<snapshot::RoomId> runtime_detected_room_ids;
};

class IFurnitureAnalysisSource {
public:
    virtual ~IFurnitureAnalysisSource() = default;
    [[nodiscard]] virtual Result<FurnitureAnalysisSourceSnapshot> Capture(
        std::uint64_t scene_generation) = 0;
};

struct FurnitureAnalysisRoom {
    std::string binding_key;
    std::optional<snapshot::RoomId> room_id;
    std::size_t resident_count{};
    std::size_t furniture_count{};
    snapshot::RoomAttributes attributes;
};

struct FurnitureAnalysisSnapshot {
    std::uint64_t scene_generation{};
    std::optional<std::int64_t> game_day;
    std::string save_identity;
    std::string binding_digest;
    std::vector<FurnitureAnalysisRoom> rooms;
    std::size_t identified_room_count{};
    std::size_t cat_count{};
    std::size_t furniture_count{};
    std::size_t placed_furniture_count{};
    std::size_t warehouse_furniture_count{};
    std::size_t furniture_info_coverage{};
    std::size_t furniture_effect_coverage{};
    furniture_planning::FurnitureLayoutPlan layout_plan;
};

}  // namespace autocattery::furniture_analysis
