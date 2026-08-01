#pragma once

#include <filesystem>
#include <span>
#include <vector>

#include "auto_cattery/snapshot/detail/furniture_attributes.hpp"
#include "auto_cattery/snapshot/detail/visual_traits.hpp"
#include "auto_cattery/snapshot/game_read_adapter.hpp"

namespace autocattery::snapshot {

struct HouseStateEntry {
    CatId cat_id{};
    RoomId room_id;
    double position_x{};
    double position_y{};
    double position_z{};
};

Result<CatSnapshot> ParseCatBlob(
    CatId cat_id,
    std::span<const std::uint8_t> blob,
    std::optional<std::int64_t> current_day,
    bool base_stats_unlocked = true,
    bool sexuality_unlocked = false);
Result<std::vector<HouseStateEntry>> ParseHouseState(
    std::span<const std::uint8_t> blob);

class SaveSnapshotAdapter final : public IGameReadAdapter {
public:
    explicit SaveSnapshotAdapter(
        std::filesystem::path save_root = {},
        std::filesystem::path game_root = {});

    Result<HouseSnapshot> CaptureHouseSnapshot(
        std::uint64_t scene_generation) override;
    Result<std::vector<HouseSnapshot>> CaptureHouseSnapshotCandidates(
        std::uint64_t scene_generation);

private:
    Result<HouseSnapshot> CaptureHouseSnapshotFromPath(
        const std::filesystem::path& save_path,
        std::uint64_t scene_generation);
    std::filesystem::path save_root_;
    std::filesystem::path game_root_;
    detail::FurnitureCatalog furniture_catalog_;
    bool furniture_catalog_attempted_{};
    detail::MutationCatalog mutation_catalog_;
    bool mutation_catalog_attempted_{};
    std::uint64_t next_snapshot_id_{1};
};

}  // namespace autocattery::snapshot
