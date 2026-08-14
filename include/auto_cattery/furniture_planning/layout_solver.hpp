#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "auto_cattery/snapshot/detail/furniture_attributes.hpp"
#include "auto_cattery/snapshot/detail/furniture_geometry.hpp"
#include "auto_cattery/furniture_planning/config.hpp"
#include "auto_cattery/room_planning/domain.hpp"

namespace autocattery::furniture_planning {

struct FurnitureLayoutMove {
    std::uint64_t stable_key{};
    std::string item_id;
    snapshot::RoomId from_room_id;
    snapshot::RoomId target_room_id;
    std::int32_t from_x{};
    std::int32_t from_y{};
    std::int32_t target_x{};
    std::int32_t target_y{};

    bool operator==(const FurnitureLayoutMove&) const = default;
};

struct FurnitureLayoutStateEdge {
    std::string binding_digest;
    FurnitureLayoutMove first_move;

    bool operator==(const FurnitureLayoutStateEdge&) const = default;
};

enum class FurnitureLayoutStateEdgeRecordStatus {
    Recorded,
    Duplicate,
    CapacityReached,
};

[[nodiscard]] FurnitureLayoutStateEdgeRecordStatus
RecordFurnitureLayoutStateEdge(
    std::vector<FurnitureLayoutStateEdge>& attempted_edges,
    const std::string& binding_digest,
    const FurnitureLayoutMove& first_move,
    std::size_t capacity = 4096U);

struct FurnitureRoomGrid {
    snapshot::RoomId room_id;
    std::size_t width{};
    std::size_t height{};
    std::vector<std::uint8_t> base_cells;
    std::vector<std::uint8_t> live_cells;
};

struct FurnitureLayoutPlan {
    snapshot::RoomId target_room_id;
    std::vector<snapshot::RoomId> exhausted_room_ids;
    std::vector<FurnitureLayoutMove> moves;
    std::size_t planned_room_count{};
    std::size_t considered_furniture_count{};
    std::size_t kept_furniture_count{};
    std::size_t warehouse_furniture_count{};
    std::size_t deferred_furniture_count{};
    std::size_t unsupported_furniture_count{};
    std::size_t no_space_furniture_count{};
    std::size_t current_state_blocked_room_count{};
    std::size_t evacuation_blocked_room_count{};
    std::size_t installation_blocked_room_count{};
    std::size_t tabu_filtered_move_count{};
};

[[nodiscard]] bool IsWarehouseLayoutMove(
    const FurnitureLayoutMove& move) noexcept;

[[nodiscard]] std::size_t FurnitureRoomPlacementOrder(
    std::string_view room_id) noexcept;

[[nodiscard]] bool IsImmediateReverseLayoutMove(
    const FurnitureLayoutMove& previous,
    const FurnitureLayoutMove& next) noexcept;

struct FurnitureReplacementPlacement {
    snapshot::RoomId room_id;
    std::int32_t x{};
    std::int32_t y{};
};

struct FurnitureSupportDependent {
    std::uint64_t stable_key{};
    std::string item_id;
    snapshot::RoomId room_id;
    std::int32_t x{};
    std::int32_t y{};
};

[[nodiscard]] std::optional<std::vector<FurnitureSupportDependent>>
FindFurnitureSupportDependentsTopDown(
    const snapshot::detail::FurniturePlacement& provider,
    const std::vector<snapshot::detail::FurniturePlacement>& furniture,
    const snapshot::detail::HouseGeometryCatalog& geometry,
    const snapshot::detail::FurnitureInfoCatalog& furniture_info,
    const std::vector<FurnitureRoomGrid>& runtime_room_grids);

[[nodiscard]] std::optional<FurnitureReplacementPlacement>
FindNearestFurnitureReplacementPlacement(
    const snapshot::detail::FurniturePlacement& placed,
    const snapshot::detail::FurniturePlacement& warehouse,
    const std::vector<snapshot::detail::FurniturePlacement>& furniture,
    const snapshot::detail::HouseGeometryCatalog& geometry,
    const snapshot::detail::FurnitureInfoCatalog& furniture_info,
    const std::vector<FurnitureRoomGrid>& runtime_room_grids);

class FurnitureLayoutSolver final {
public:
    [[nodiscard]] FurnitureLayoutPlan Plan(
        const std::vector<snapshot::detail::FurniturePlacement>& furniture,
        const snapshot::detail::HouseGeometryCatalog& geometry,
        const snapshot::detail::FurnitureInfoCatalog& furniture_info,
        const std::vector<FurnitureRoomGrid>& runtime_room_grids = {},
        const std::vector<snapshot::RoomId>& locked_room_ids = {},
        const snapshot::detail::FurnitureCatalog& furniture_effects = {},
        const std::vector<room_planning::RoomPurposeAssignment>&
            room_purposes = {},
        const FurniturePlacementConfig& placement_config = {},
        const std::vector<FurnitureLayoutMove>& forbidden_moves = {},
        const snapshot::RoomId& preferred_focus_room_id = {}) const;
};

}  // namespace autocattery::furniture_planning
