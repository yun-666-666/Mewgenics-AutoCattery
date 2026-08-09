#pragma once

#include "auto_cattery/snapshot/domain.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace autocattery::snapshot::detail {

inline constexpr std::size_t kFurnitureInfoOpaquePayloadSize = 580;
inline constexpr std::size_t kFurniturePlacementGridOffset = 4;
inline constexpr std::size_t kFurniturePlacementGridWidth = 24;
inline constexpr std::size_t kFurniturePlacementGridHeight = 24;
inline constexpr std::size_t kFurniturePlacementGridCellCount =
    kFurniturePlacementGridWidth * kFurniturePlacementGridHeight;
static_assert(
    kFurniturePlacementGridOffset + kFurniturePlacementGridCellCount ==
    kFurnitureInfoOpaquePayloadSize);

enum class FurniturePlacementTile : std::uint8_t {
    Empty = 0,
    Hitbox = 1,
    Solid = 2,
    Support = 3,
    Surface = 4,
    PoopLogic = 5
};

struct FurniturePlacementGrid {
    bool supported{};
    std::array<
        FurniturePlacementTile,
        kFurniturePlacementGridCellCount> tiles{};

    [[nodiscard]] FurniturePlacementTile At(
        std::size_t x,
        std::size_t y) const noexcept {
        return tiles[y * kFurniturePlacementGridWidth + x];
    }

    [[nodiscard]] std::size_t Count(
        FurniturePlacementTile tile) const noexcept {
        return static_cast<std::size_t>(std::count(
            tiles.begin(), tiles.end(), tile));
    }
};

struct RoomGeometryDefinition {
    std::string definition_id;
    RoomId room_id;
    std::int32_t width{};
    std::int32_t height{};
    std::vector<std::vector<std::int32_t>> built_in_collision;
};

struct RoomCollisionGrid {
    bool supported{};
    std::size_t width{};
    std::size_t height{};
    std::vector<std::uint8_t> cells;

    [[nodiscard]] std::uint8_t At(
        std::size_t x,
        std::size_t y) const noexcept {
        return cells[y * width + x];
    }
};

struct HouseRoomPosition {
    std::string room_definition_id;
    RoomId room_id;
    double x{};
    double y{};
};

struct HouseLayoutDefinition {
    std::string house_id;
    std::vector<HouseRoomPosition> room_positions;
};

struct HouseGeometryCatalog {
    std::vector<RoomGeometryDefinition> rooms;
    std::vector<HouseLayoutDefinition> houses;
};

struct FurnitureInfoRecord {
    std::string item_id;
    std::uint32_t unknown_after_name_length{};
    std::array<std::byte, kFurnitureInfoOpaquePayloadSize> opaque_payload{};
    FurniturePlacementGrid placement_grid;
    std::size_t nonzero_bytes_outside_placement_grid{};
};

struct FurnitureInfoCatalog {
    std::uint32_t format_version{};
    std::vector<FurnitureInfoRecord> records;
};

[[nodiscard]] bool LoadHouseGeometryCatalog(
    const std::filesystem::path& gpak_path,
    HouseGeometryCatalog& catalog,
    std::string& error);

[[nodiscard]] bool LoadFurnitureInfoCatalog(
    const std::filesystem::path& gpak_path,
    FurnitureInfoCatalog& catalog,
    std::string& error);

[[nodiscard]] RoomCollisionGrid DecodeRoomCollisionGrid(
    const RoomGeometryDefinition& room);

}  // namespace autocattery::snapshot::detail
