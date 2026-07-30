#include "auto_cattery/snapshot/save_snapshot_adapter.hpp"

#include "auto_cattery/snapshot/detail/save_database.hpp"
#include "auto_cattery/snapshot/detail/save_locator.hpp"
#include "auto_cattery/snapshot/detail/snapshot_assembler.hpp"

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
    std::filesystem::path save_root)
    : save_root_(std::move(save_root)) {}

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
            ParseCatBlob(stored_cat.id, AsBytes(stored_cat.blob), day);
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
        house_entries.value);
    if (snapshot) {
        ++next_snapshot_id_;
    }
    return snapshot;
}

}  // namespace autocattery::snapshot
