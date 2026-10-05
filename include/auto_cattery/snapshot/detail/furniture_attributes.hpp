#pragma once

#include <span>

#include "auto_cattery/snapshot/detail/save_database.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace autocattery::snapshot::detail {

struct FurniturePlacement {
    std::string item_id;
    RoomId room_id;
    std::uint64_t flags{};
};

using FurnitureCatalog =
    std::unordered_map<std::string, RoomAttributes>;

[[nodiscard]] bool LoadFurnitureCatalog(
    const std::filesystem::path& gpak_path,
    FurnitureCatalog& catalog,
    std::string& error);

[[nodiscard]] bool ParseFurniturePlacements(
    const std::vector<FurnitureStorageRecord>& records,
    std::vector<FurniturePlacement>& placements,
    std::string& error);

// Read confirmed upgrades from the current house_unlocks save record.
bool ApplyUnlockedHouseRooms(HouseSnapshot& snapshot, std::span<const std::byte> bytes);

void ApplyFurnitureRoomAttributes(
    HouseSnapshot& snapshot,
    const std::vector<FurniturePlacement>& placements,
    const FurnitureCatalog& catalog);

}  // namespace autocattery::snapshot::detail
