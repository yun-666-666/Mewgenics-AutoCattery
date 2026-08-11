#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "auto_cattery/error.hpp"
#include "auto_cattery/snapshot/detail/furniture_attributes.hpp"
#include "auto_cattery/snapshot/domain.hpp"

namespace autocattery::ui {

using RuntimePointer = std::uintptr_t;

struct RuntimeCatRoomState {
    snapshot::CatId cat_id{};
    RuntimePointer component{};
    RuntimePointer room{};
};

struct RuntimeRoomEvidence {
    RuntimePointer room{};
    std::vector<snapshot::RoomId> detected_ids;
};

struct RuntimeHouseState {
    std::size_t available_room_count{};
    std::vector<RuntimeCatRoomState> cats;
    std::vector<RuntimeRoomEvidence> rooms;
};

struct RuntimeFurniturePlacementState {
    std::uint64_t stable_key{};
    std::string item_id;
    snapshot::RoomId room_id;
    std::int32_t position_x{};
    std::int32_t position_y{};
    std::int32_t scale_x{};
    std::int32_t scale_y{};
};

struct RuntimeFurnitureRoomGridState {
    snapshot::RoomId room_id;
    std::size_t width{};
    std::size_t height{};
    std::vector<std::uint8_t> base_cells;
    std::vector<std::uint8_t> live_cells;
};

struct RuntimeWarehouseFurniturePieceState {
    std::uint64_t stable_key{};
    std::string item_id;
};

struct RuntimeFurnitureState {
    std::size_t scene_piece_count{};
    std::vector<RuntimeFurniturePlacementState> placements;
    std::vector<RuntimeWarehouseFurniturePieceState> warehouse_pieces;
    std::vector<RuntimeFurnitureRoomGridState> room_grids;
};

[[nodiscard]] Result<std::unordered_map<snapshot::RoomId, RuntimePointer>>
ResolveRuntimeRoomPointers(
    const snapshot::HouseSnapshot& snapshot,
    const RuntimeHouseState& runtime);

[[nodiscard]] Result<void> OverlayRuntimeHouseState(
    snapshot::HouseSnapshot& snapshot,
    const RuntimeHouseState& runtime);

[[nodiscard]] Result<void> OverlayRuntimeHouseState(
    snapshot::HouseSnapshot& snapshot,
    const RuntimeHouseState& runtime,
    const std::unordered_map<snapshot::RoomId, RuntimePointer>& room_pointers);

[[nodiscard]] bool RuntimeHouseStateMatches(
    const snapshot::HouseSnapshot& snapshot,
    const RuntimeHouseState& runtime);

[[nodiscard]] Result<void> OverlayRuntimeFurnitureState(
    std::vector<snapshot::detail::FurniturePlacement>& furniture,
    const RuntimeFurnitureState& runtime);

}  // namespace autocattery::ui
