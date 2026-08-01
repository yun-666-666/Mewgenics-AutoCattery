#include "runtime_matched_save_snapshot_adapter.hpp"

#include <algorithm>
#include <array>
#include <sstream>

#include "auto_cattery/logger.hpp"

namespace autocattery::ui {
namespace {

constexpr std::array<const char*, 4> kAvailableRoomOrder{
    "Floor1_Large",
    "Attic",
    "Floor1_Small",
    "Floor2_Large"
};

void AddAvailableEmptyRooms(
    snapshot::HouseSnapshot& snapshot,
    std::size_t available_room_count) {
    const auto count =
        std::min(available_room_count, kAvailableRoomOrder.size());
    for (std::size_t index = 0; index < count; ++index) {
        const auto* id = kAvailableRoomOrder[index];
        if (std::ranges::none_of(
                snapshot.rooms,
                [id](const auto& room) {
                    return room.id == id;
                })) {
            snapshot.rooms.push_back({.id = id});
        }
    }
}

}  // namespace

void RuntimeMatchedSaveSnapshotAdapter::SetRuntimeContext(
    std::size_t house_cat_count,
    std::size_t available_room_count) noexcept {
    std::scoped_lock lock(context_mutex_);
    house_cat_count_ = house_cat_count;
    available_room_count_ = available_room_count;
    runtime_state_.reset();
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
    {
        std::scoped_lock lock(context_mutex_);
        expected_cats = house_cat_count_;
        expected_rooms = available_room_count_;
        runtime_state = runtime_state_;
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
    AddAvailableEmptyRooms(snapshot, expected_rooms);
    if (runtime_state) {
        const auto overlaid =
            OverlayRuntimeHouseState(snapshot, *runtime_state);
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
