#include "runtime_matched_save_snapshot_adapter.hpp"

#include <algorithm>
#include <sstream>

#include "auto_cattery/logger.hpp"

namespace autocattery::ui {
RuntimeMatchedSaveSnapshotAdapter::RuntimeMatchedSaveSnapshotAdapter(
    std::filesystem::path game_root)
    : saves_({}, std::move(game_root)) {}

void RuntimeMatchedSaveSnapshotAdapter::SetRuntimeContext(
    std::size_t house_cat_count,
    std::size_t available_room_count) noexcept {
    std::scoped_lock lock(context_mutex_);
    house_cat_count_ = house_cat_count;
    available_room_count_ = available_room_count;
    runtime_state_.reset();
    room_mapping_.reset();
    room_mapping_generation_ = 0;
}

void RuntimeMatchedSaveSnapshotAdapter::SetRuntimeHouseState(
    RuntimeHouseState state) noexcept {
    std::scoped_lock lock(context_mutex_);
    house_cat_count_ = state.cats.size();
    available_room_count_ = state.available_room_count;
    runtime_state_ = std::move(state);
}

Result<snapshot::HouseSnapshot>
RuntimeMatchedSaveSnapshotAdapter::CaptureHouseSnapshot(
    std::uint64_t scene_generation) {
    std::size_t expected_cats{};
    std::size_t expected_rooms{};
    std::optional<RuntimeHouseState> runtime_state;
    std::optional<std::unordered_map<
        snapshot::RoomId, RuntimePointer>> room_mapping;
    {
        std::scoped_lock lock(context_mutex_);
        expected_cats = house_cat_count_;
        expected_rooms = available_room_count_;
        runtime_state = runtime_state_;
        if (room_mapping_generation_ == scene_generation) {
            room_mapping = room_mapping_;
        }
    }
    if (expected_cats == 0U || expected_rooms < 2U) {
        return {
            {},
            ErrorCode::SceneUnavailable,
            "current House runtime context is unavailable"
        };
    }

    auto candidates =
        saves_.CaptureHouseSnapshotCandidates(scene_generation);
    if (!candidates) {
        return {
            {},
            candidates.code,
            candidates.message
        };
    }

    std::ostringstream observed;
    for (std::size_t index = 0;
         index < candidates.value.size();
         ++index) {
        if (index != 0U) {
            observed << ',';
        }
        observed << candidates.value[index].cats.size();
    }
    Logger::Instance().Write(
        LogLevel::Info,
        "RuntimeSaveSelection",
        "AC14314",
        "Runtime cats=" + std::to_string(expected_cats) +
            ", rooms=" + std::to_string(expected_rooms) +
            ", save candidate cat counts=" + observed.str());

    const auto selected = std::ranges::find_if(
        candidates.value,
        [expected_cats](const auto& candidate) {
            return candidate.cats.size() == expected_cats;
        });
    if (selected == candidates.value.end()) {
        return {
            {},
            ErrorCode::CatDataUnavailable,
            "no save snapshot matches the current House cat count"
        };
    }

    auto snapshot = std::move(*selected);
    if (runtime_state) {
        if (!room_mapping) {
            const auto resolved =
                ResolveRuntimeRoomPointers(snapshot, *runtime_state);
            if (!resolved) {
                return {{}, resolved.code, resolved.message};
            }
            room_mapping = resolved.value;
            std::scoped_lock lock(context_mutex_);
            room_mapping_ = room_mapping;
            room_mapping_generation_ = scene_generation;
        }
        const auto overlaid = OverlayRuntimeHouseState(
            snapshot, *runtime_state, *room_mapping);
        if (!overlaid) {
            return {{}, overlaid.code, overlaid.message};
        }
        Logger::Instance().Write(
            LogLevel::Info,
            "RuntimeSaveSelection",
            "AC14317",
            "Preview snapshot overlaid with current runtime rooms: cats=" +
                std::to_string(snapshot.cats.size()) +
                ", rooms=" + std::to_string(snapshot.rooms.size()));
    }
    return {std::move(snapshot)};
}

}  // namespace autocattery::ui
