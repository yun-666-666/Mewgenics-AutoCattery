#include "auto_cattery/snapshot/save_snapshot_adapter.hpp"

#include "auto_cattery/snapshot/detail/save_database.hpp"
#include "auto_cattery/snapshot/detail/furniture_attributes.hpp"
#include "auto_cattery/snapshot/detail/unlocked_breeding_data.hpp"
#include "auto_cattery/snapshot/detail/save_locator.hpp"
#include "auto_cattery/snapshot/detail/snapshot_assembler.hpp"
#include "auto_cattery/snapshot/detail/visual_traits.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_set>
#include <utility>

namespace autocattery::snapshot {
namespace {

std::span<const std::uint8_t> AsBytes(
    const std::vector<std::byte>& bytes) {
    return {
        reinterpret_cast<const std::uint8_t*>(bytes.data()),
        bytes.size()
    };
}

Result<HouseSnapshot> Failure(
    ErrorCode code,
    std::string prefix,
    const std::string& detail) {
    if (!detail.empty()) {
        prefix.append(": ").append(detail);
    }
    return {{}, code, std::move(prefix)};
}

}  // namespace

SaveSnapshotAdapter::SaveSnapshotAdapter(
    std::filesystem::path save_root,
    std::filesystem::path game_root)
    : save_root_(std::move(save_root)),
      game_root_(std::move(game_root)) {}

Result<HouseSnapshot> SaveSnapshotAdapter::CaptureHouseSnapshot(
    std::uint64_t scene_generation) {
    std::string error;
    const auto save_path =
        detail::FindMostRecentSave(save_root_, error);
    if (!save_path) {
        return Failure(
            ErrorCode::CatDataUnavailable,
            "save discovery failed",
            error);
    }
    return CaptureHouseSnapshotFromPath(*save_path, scene_generation);
}

Result<std::vector<HouseSnapshot>>
SaveSnapshotAdapter::CaptureHouseSnapshotCandidates(
    std::uint64_t scene_generation) {
    std::string error;
    const auto save_paths =
        detail::FindSaveCandidates(save_root_, error);
    if (save_paths.empty()) {
        return {
            {},
            ErrorCode::CatDataUnavailable,
            "save discovery failed: " + error
        };
    }

    std::vector<HouseSnapshot> snapshots;
    std::string first_failure;
    for (const auto& save_path : save_paths) {
        auto captured = CaptureHouseSnapshotFromPath(
            save_path,
            scene_generation);
        if (captured) {
            snapshots.push_back(std::move(captured.value));
        } else if (first_failure.empty()) {
            first_failure = captured.message;
        }
    }
    if (snapshots.empty()) {
        return {
            {},
            ErrorCode::CatDataUnavailable,
            "no readable save candidate: " + first_failure
        };
    }
    return {std::move(snapshots)};
}

Result<HouseSnapshot> SaveSnapshotAdapter::CaptureHouseSnapshotFromPath(
    const std::filesystem::path& save_path,
    std::uint64_t scene_generation) {
    std::string error;

    auto database =
        detail::SaveDatabase::OpenReadOnly(save_path, error);
    if (!database) {
        return Failure(
            ErrorCode::CatDataUnavailable,
            "save database unavailable",
            error);
    }

    std::optional<std::int32_t> current_day;
    std::vector<detail::CatStorageRecord> stored_cats;
    std::vector<detail::FurnitureStorageRecord> stored_furniture;
    std::optional<std::vector<std::byte>> stored_house_state;
    if (!database->ReadCurrentDay(current_day, error) ||
        !database->ReadCats(stored_cats, error)) {
        return Failure(
            ErrorCode::CatDataUnavailable,
            "save query failed",
            error);
    }
    if (!database->ReadHouseState(stored_house_state, error) ||
        !stored_house_state.has_value()) {
        return Failure(
            ErrorCode::RoomDataUnavailable,
            "house state unavailable",
            error);
    }
    const auto breeding_data =
        detail::LoadUnlockedBreedingData(*database);
    if (!breeding_data) {
        return {{}, breeding_data.code, breeding_data.message};
    }
    std::vector<detail::FurniturePlacement> furniture;
    const bool furniture_read =
        database->ReadFurniture(stored_furniture, error) &&
        detail::ParseFurniturePlacements(
            stored_furniture, furniture, error);
    if (!furniture_catalog_attempted_) {
        furniture_catalog_attempted_ = true;
        std::string catalog_error;
        const bool catalog_loaded = detail::LoadFurnitureCatalog(
            game_root_ / L"resources.gpak",
            furniture_catalog_,
            catalog_error);
        if (!catalog_loaded) {
            furniture_catalog_.clear();
        }
    }
    if (!mutation_catalog_attempted_) {
        mutation_catalog_attempted_ = true;
        std::string catalog_error;
        if (!detail::LoadMutationCatalog(
                game_root_ / L"resources.gpak",
                mutation_catalog_,
                catalog_error)) {
            mutation_catalog_.clear();
        }
    }

    const auto house_entries =
        ParseHouseState(AsBytes(*stored_house_state));
    if (!house_entries) {
        return Failure(
            house_entries.code,
            "house state parsing failed",
            house_entries.message);
    }
    std::unordered_set<CatId> current_house_cat_ids;
    for (const auto& entry : house_entries.value) {
        current_house_cat_ids.insert(entry.cat_id);
    }

    const std::optional<std::int64_t> day =
        current_day
            ? std::optional<std::int64_t>(*current_day)
            : std::nullopt;
    std::vector<CatSnapshot> cats;
    cats.reserve(current_house_cat_ids.size());
    for (std::size_t index = 0; index < stored_cats.size(); ++index) {
        const auto& stored_cat = stored_cats[index];
        if (!current_house_cat_ids.contains(stored_cat.id)) {
            continue;
        }
        auto parsed =
            ParseCatBlob(
                stored_cat.id,
                AsBytes(stored_cat.blob),
                day,
                true,
                true); // Verified format-19 stats/personality exist before Tink UI unlocks.
        if (!parsed) {
            return Failure(
                parsed.code,
                "cat parsing failed at record " +
                    std::to_string(index + 1),
                parsed.message);
        }
        cats.push_back(std::move(parsed.value));
    }

    auto snapshot = detail::AssembleHouseSnapshot(
        next_snapshot_id_,
        scene_generation,
        day,
        save_path.filename().string(),
        std::move(cats),
        house_entries.value,
        breeding_data.value.pedigree
            ? &*breeding_data.value.pedigree : nullptr,
        true,
        true);
    if (snapshot) {
        std::optional<std::vector<std::byte>> house_unlocks;
        if (!database->ReadFileBlob("house_unlocks", house_unlocks, error) ||
            (house_unlocks && !detail::ApplyUnlockedHouseRooms(snapshot.value, *house_unlocks))) {
            return Failure(ErrorCode::RoomDataUnavailable, "house room unlocks unavailable", error);
        }
        if (!mutation_catalog_.empty()) {
            for (auto& cat : snapshot.value.cats) {
                detail::ApplyMutationCatalog(cat, mutation_catalog_);
            }
            snapshot.value.capabilities.read_visual_traits =
                std::ranges::all_of(
                    snapshot.value.cats,
                    [](const CatSnapshot& cat) {
                        return cat.raw_visual_part_slots.size() == 15;
                    });
        }
        if (furniture_read && !furniture_catalog_.empty()) {
            detail::ApplyFurnitureRoomAttributes(
                snapshot.value, furniture, furniture_catalog_);
        }
        ++next_snapshot_id_;
    }
    return snapshot;
}

}  // namespace autocattery::snapshot
