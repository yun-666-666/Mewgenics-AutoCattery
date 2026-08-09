#pragma once

#include "auto_cattery/snapshot/domain.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace autocattery::snapshot::detail {

inline constexpr std::size_t kFurnitureInfoOpaquePayloadSize = 580;

struct RoomGeometryDefinition {
    std::string definition_id;
    RoomId room_id;
    std::int32_t width{};
    std::int32_t height{};
    std::vector<std::vector<std::int32_t>> built_in_collision;
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

}  // namespace autocattery::snapshot::detail
