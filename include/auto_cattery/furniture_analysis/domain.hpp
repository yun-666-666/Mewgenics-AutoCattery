#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "auto_cattery/error.hpp"
#include "auto_cattery/furniture_planning/layout_solver.hpp"
#include "auto_cattery/room_planning/domain.hpp"
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
    std::vector<furniture_planning::FurnitureRoomGrid> runtime_room_grids;
    std::size_t runtime_scene_piece_count{};
    std::size_t runtime_placed_piece_count{};
    struct WarehousePieceEvidence {
        std::uint64_t stable_key{};
        std::string item_id;
    };
    std::vector<WarehousePieceEvidence> runtime_warehouse_pieces;
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

struct FurnitureAttributeUpgrade {
    std::uint64_t placed_stable_key{};
    std::uint64_t warehouse_stable_key{};
    std::string placed_item_id;
    std::string warehouse_item_id;
    snapshot::RoomId target_room_id;
    std::int32_t original_x{};
    std::int32_t original_y{};
    std::int32_t target_x{};
    std::int32_t target_y{};
    std::vector<furniture_planning::FurnitureSupportDependent>
        support_dependents_top_down;
    snapshot::RoomAttributes gain;
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
    std::size_t runtime_scene_piece_count{};
    std::size_t runtime_placed_piece_count{};
    std::size_t runtime_warehouse_piece_count{};
    std::size_t runtime_warehouse_piece_match_count{};
    std::vector<FurnitureAttributeUpgrade> attribute_upgrades;
    snapshot::RoomAttributes attribute_upgrade_gain;
    std::vector<room_planning::RoomPurposeAssignment> room_purposes;
    furniture_planning::FurnitureLayoutPlan layout_plan;
};

}  // namespace autocattery::furniture_analysis
