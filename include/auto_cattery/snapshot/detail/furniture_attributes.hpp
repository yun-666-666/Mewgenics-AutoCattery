#pragma once

#include "auto_cattery/snapshot/detail/save_database.hpp"

#include <filesystem>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace autocattery::snapshot::detail {

enum class FurniturePlacementFlag : std::uint64_t {
    Rare = 0x2
};

inline constexpr std::uint64_t kKnownFurniturePlacementFlags =
    static_cast<std::uint64_t>(FurniturePlacementFlag::Rare);

struct FurniturePlacement {
    std::int64_t instance_id{};
    std::uint32_t format_version{};
    std::string item_id;
    std::uint32_t unknown_after_item_length{};
    std::uint64_t placement_flags{};
    RoomId room_id;
    std::uint32_t unknown_after_room_length{};
    std::int32_t position_x{};
    std::int32_t position_y{};
    std::uint32_t position_z{};
    std::uint32_t unknown_flag_1{};
    std::uint32_t unknown_flag_2{};

    [[nodiscard]] bool HasPlacementFlag(
        FurniturePlacementFlag flag) const noexcept {
        return (placement_flags & static_cast<std::uint64_t>(flag)) != 0U;
    }

    [[nodiscard]] bool HasOnlyKnownPlacementFlags() const noexcept {
        return (placement_flags & ~kKnownFurniturePlacementFlags) == 0U;
    }

    [[nodiscard]] bool IsRare() const noexcept {
        return HasPlacementFlag(FurniturePlacementFlag::Rare);
    }
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

void ApplyFurnitureRoomAttributes(
    HouseSnapshot& snapshot,
    const std::vector<FurniturePlacement>& placements,
    const FurnitureCatalog& catalog);

}  // namespace autocattery::snapshot::detail
