#include "runtime_matched_save_snapshot_adapter.hpp"

#include <algorithm>
#include <array>
#include <sstream>
#include <unordered_set>

#include "auto_cattery/logger.hpp"

namespace autocattery::ui {
namespace {

constexpr std::array<const char*, 5> kAvailableRoomOrder{
    "Floor1_Large",
    "Attic",
    "Floor1_Small",
    "Floor2_Large",
    "Floor2_Small"
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
            snapshot::RoomSnapshot room{.id = id};
            if (snapshot.capabilities.read_room_attributes) {
                room.attributes = snapshot::RoomAttributes{};
            }
            snapshot.rooms.push_back(std::move(room));
        }
    }
}

}  // namespace

RuntimeMatchedSaveSnapshotAdapter::RuntimeMatchedSaveSnapshotAdapter(
    std::filesystem::path game_root)
    : saves_({}, game_root), game_root_(std::move(game_root)) {}

void RuntimeMatchedSaveSnapshotAdapter::SetRuntimeContext(
    std::size_t house_cat_count,
    std::size_t available_room_count,
    RuntimeFurnitureState furniture) noexcept {
    std::scoped_lock lock(context_mutex_);
    house_cat_count_ = house_cat_count;
    available_room_count_ = available_room_count;
    runtime_state_.reset();
    runtime_furniture_state_ = std::move(furniture);
    room_mapping_.reset();
    room_mapping_generation_ = 0;
}

void RuntimeMatchedSaveSnapshotAdapter::SetRuntimeHouseState(
    RuntimeHouseState state,
    RuntimeFurnitureState furniture) noexcept {
    std::scoped_lock lock(context_mutex_);
    house_cat_count_ = state.cats.size();
    available_room_count_ = state.available_room_count;
    runtime_state_ = std::move(state);
    runtime_furniture_state_ = std::move(furniture);
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
    AddAvailableEmptyRooms(snapshot, expected_rooms);
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

Result<furniture_analysis::FurnitureAnalysisSourceSnapshot>
RuntimeMatchedSaveSnapshotAdapter::Capture(
    std::uint64_t scene_generation) {
    std::size_t expected_cats{};
    std::size_t expected_rooms{};
    std::optional<RuntimeHouseState> runtime_state;
    std::optional<RuntimeFurnitureState> runtime_furniture_state;
    {
        std::scoped_lock lock(context_mutex_);
        expected_cats = house_cat_count_;
        expected_rooms = available_room_count_;
        runtime_state = runtime_state_;
        runtime_furniture_state = runtime_furniture_state_;
    }
    if (expected_cats == 0U || expected_rooms == 0U ||
        !runtime_furniture_state) {
        return {{}, ErrorCode::SceneUnavailable,
                "current House furniture analysis context is unavailable"};
    }

    auto candidates =
        saves_.CaptureFurnitureAnalysisCandidates(scene_generation);
    if (!candidates) {
        return {{}, candidates.code, candidates.message};
    }
    const auto exact_runtime_match = [&runtime_state](const auto& candidate) {
        if (!runtime_state ||
            candidate.house.cats.size() != runtime_state->cats.size()) {
            return false;
        }
        std::unordered_set<snapshot::CatId> expected;
        for (const auto& cat : runtime_state->cats) {
            expected.insert(cat.cat_id);
        }
        return std::ranges::all_of(
            candidate.house.cats,
            [&expected](const auto& cat) {
                return expected.contains(cat.id);
            });
    };
    auto selected = std::ranges::find_if(
        candidates.value, exact_runtime_match);
    if (selected == candidates.value.end()) {
        selected = std::ranges::find_if(
            candidates.value,
            [expected_cats](const auto& candidate) {
                return candidate.house.cats.size() == expected_cats;
            });
    }
    if (selected == candidates.value.end()) {
        return {{}, ErrorCode::CatDataUnavailable,
                "no save snapshot matches the current House identity"};
    }

    furniture_analysis::FurnitureAnalysisSourceSnapshot source;
    source.house = std::move(selected->house);
    source.furniture = std::move(selected->furniture);
    source.available_room_count = expected_rooms;
    source.runtime_scene_piece_count =
        runtime_furniture_state->scene_piece_count;
    source.runtime_placed_piece_count =
        runtime_furniture_state->placements.size();
    source.runtime_warehouse_pieces.reserve(
        runtime_furniture_state->warehouse_pieces.size());
    for (const auto& piece : runtime_furniture_state->warehouse_pieces) {
        source.runtime_warehouse_pieces.push_back({
            .stable_key = piece.stable_key,
            .item_id = piece.item_id});
    }
    source.runtime_room_grids.reserve(
        runtime_furniture_state->room_grids.size());
    for (const auto& grid : runtime_furniture_state->room_grids) {
        source.runtime_room_grids.push_back({
            .room_id = grid.room_id,
            .width = grid.width,
            .height = grid.height,
            .base_cells = grid.base_cells,
            .live_cells = grid.live_cells});
    }
    const auto furniture_overlaid = OverlayRuntimeFurnitureState(
        source.furniture, *runtime_furniture_state);
    if (!furniture_overlaid) {
        return {{}, furniture_overlaid.code,
                "current House furniture overlay failed: " +
                    furniture_overlaid.message};
    }
    Logger::Instance().Write(
        LogLevel::Info,
        "RuntimeSaveSelection",
        "AC14319",
        "Furniture analysis snapshot overlaid from current runtime: furniture=" +
            std::to_string(runtime_furniture_state->placements.size()) +
            ", scene pieces=" +
            std::to_string(runtime_furniture_state->scene_piece_count) +
            ", warehouse pieces=" +
            std::to_string(runtime_furniture_state->warehouse_pieces.size()));
    if (runtime_state) {
        std::unordered_set<snapshot::RoomId> seen;
        for (const auto& room : source.house.rooms) {
            seen.insert(room.id);
        }
        for (const auto& room : runtime_state->rooms) {
            for (const auto& id : room.detected_ids) {
                if (id.empty()) {
                    continue;
                }
                source.runtime_detected_room_ids.push_back(id);
                if (seen.insert(id).second) {
                    source.house.rooms.push_back({.id = id});
                }
            }
        }
        const auto overlaid = OverlayRuntimeHouseState(
            source.house, *runtime_state);
        if (!overlaid) {
            return {{}, overlaid.code,
                    "current House furniture analysis overlay failed: " +
                        overlaid.message};
        }
        std::ranges::sort(source.runtime_detected_room_ids);
        source.runtime_detected_room_ids.erase(
            std::unique(
                source.runtime_detected_room_ids.begin(),
                source.runtime_detected_room_ids.end()),
            source.runtime_detected_room_ids.end());
    }

    std::string error;
    const auto resources = game_root_ / L"resources.gpak";
    if (!snapshot::detail::LoadHouseGeometryCatalog(
            resources, source.geometry, error) ||
        !snapshot::detail::LoadFurnitureInfoCatalog(
            resources, source.furniture_info, error) ||
        !snapshot::detail::LoadFurnitureCatalog(
            resources, source.furniture_effects, error)) {
        return {{}, ErrorCode::RoomDataUnavailable,
                "furniture analysis resource capture failed: " + error};
    }
    Logger::Instance().Write(
        LogLevel::Info,
        "FurnitureAnalysis",
        "AC3201",
        "Read-only source captured: cats=" +
            std::to_string(source.house.cats.size()) +
            ", rooms=" + std::to_string(source.available_room_count) +
            ", furniture=" + std::to_string(source.furniture.size()) +
            ".");
    return {std::move(source)};
}

}  // namespace autocattery::ui
