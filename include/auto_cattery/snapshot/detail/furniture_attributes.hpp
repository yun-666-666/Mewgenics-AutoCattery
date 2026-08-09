#pragma once

#include "auto_cattery/snapshot/detail/save_database.hpp"

#include <filesystem>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace autocattery::snapshot::detail {

enum class FurniturePlacementFlag : std::uint64_t {
    Rare = 0x2
};

inline constexpr std::uint64_t kKnownFurniturePlacementFlags =
    static_cast<std::uint64_t>(FurniturePlacementFlag::Rare);

struct FurnitureGridCoordinate {
    std::int32_t x{};
    std::int32_t y{};
};

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
    std::int32_t scale_x{};
    std::int32_t scale_y{};

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

    [[nodiscard]] bool HasSupportedGridScale() const noexcept {
        const auto supported_axis = [](std::int32_t value) {
            return value == -1 || value == 1;
        };
        return supported_axis(scale_x) && supported_axis(scale_y);
    }

    [[nodiscard]] std::optional<FurnitureGridCoordinate> MapGridCellToRoom(
        std::int32_t local_x,
        std::int32_t local_y) const noexcept {
        if (!HasSupportedGridScale()) {
            return std::nullopt;
        }
        const auto mapped_x = static_cast<std::int64_t>(position_x) +
            static_cast<std::int64_t>(scale_x) * local_x;
        const auto mapped_y = static_cast<std::int64_t>(position_y) +
            static_cast<std::int64_t>(scale_y) * local_y;
        constexpr auto minimum =
            static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min());
        constexpr auto maximum =
            static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max());
        if (mapped_x < minimum || mapped_x > maximum ||
            mapped_y < minimum || mapped_y > maximum) {
            return std::nullopt;
        }
        return FurnitureGridCoordinate{
            .x = static_cast<std::int32_t>(mapped_x),
            .y = static_cast<std::int32_t>(mapped_y)};
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
