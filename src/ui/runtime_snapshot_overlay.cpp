#include "runtime_house_state.hpp"

#include <algorithm>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace autocattery::ui {

Result<void> OverlayRuntimeHouseState(
    snapshot::HouseSnapshot& snapshot,
    const RuntimeHouseState& runtime) {
    const auto room_pointers =
        ResolveRuntimeRoomPointers(snapshot, runtime);
    if (!room_pointers) {
        return {room_pointers.code, room_pointers.message};
    }
    return OverlayRuntimeHouseState(snapshot, runtime, room_pointers.value);
}

Result<void> OverlayRuntimeHouseState(
    snapshot::HouseSnapshot& snapshot,
    const RuntimeHouseState& runtime,
    const std::unordered_map<snapshot::RoomId, RuntimePointer>& room_pointers) {
    std::unordered_map<RuntimePointer, snapshot::RoomId> room_ids;
    for (const auto& [id, pointer] : room_pointers) {
        if (!room_ids.emplace(pointer, id).second) {
            return {ErrorCode::RoomDataUnavailable,
                    "runtime rooms are not one-to-one"};
        }
    }
    std::unordered_map<snapshot::CatId, RuntimePointer> current_rooms;
    for (const auto& cat : runtime.cats) {
        current_rooms.emplace(cat.cat_id, cat.room);
    }
    for (auto& room : snapshot.rooms) {
        room.residents.clear();
    }
    for (auto& cat : snapshot.cats) {
        const auto current = current_rooms.find(cat.id);
        if (current != current_rooms.end() && current->second == 0) {
            cat.room_id.reset();
            cat.in_adventure_box = false;
            continue;
        }
        const auto room = current == current_rooms.end()
            ? room_ids.end()
            : room_ids.find(current->second);
        if (room == room_ids.end()) {
            return {ErrorCode::RoomDataUnavailable,
                    "a runtime cat room is unknown"};
        }
        cat.room_id = room->second;
        cat.in_adventure_box = room->second == "AdventureBox";
        const auto target = std::ranges::find_if(
            snapshot.rooms,
            [&room](const auto& candidate) {
                return candidate.id == room->second;
            });
        if (target == snapshot.rooms.end()) {
            return {ErrorCode::RoomDataUnavailable,
                    "a runtime room is unavailable"};
        }
        target->residents.push_back(cat.id);
    }
    const auto validation = snapshot::Validate(snapshot);
    return validation.Valid()
        ? Result<void>{}
        : Result<void>{ErrorCode::SnapshotInvalid,
                       "runtime room overlay failed validation"};
}

bool RuntimeHouseStateMatches(
    const snapshot::HouseSnapshot& snapshot,
    const RuntimeHouseState& runtime) {
    auto current = snapshot;
    if (!OverlayRuntimeHouseState(current, runtime)) {
        return false;
    }
    for (const auto& cat : snapshot.cats) {
        const auto observed = std::ranges::find_if(
            current.cats,
            [&cat](const auto& candidate) {
                return candidate.id == cat.id;
            });
        if (observed == current.cats.end() ||
            observed->room_id != cat.room_id) {
            return false;
        }
    }
    return true;
}

Result<void> OverlayRuntimeFurnitureState(
    std::vector<snapshot::detail::FurniturePlacement>& furniture,
    const RuntimeFurnitureState& runtime) {
    using Placement = snapshot::detail::FurniturePlacement;
    std::unordered_map<std::uint64_t, Placement*> saved_by_key;
    saved_by_key.reserve(furniture.size());
    for (auto& placement : furniture) {
        if (placement.instance_id <= 0 ||
            !saved_by_key.emplace(
                static_cast<std::uint64_t>(placement.instance_id),
                &placement).second) {
            return {ErrorCode::SnapshotInvalid,
                    "saved furniture identity is incomplete or ambiguous"};
        }
    }

    std::unordered_set<std::uint64_t> runtime_keys;
    runtime_keys.reserve(runtime.placements.size());
    for (const auto& current : runtime.placements) {
        if (current.stable_key == 0U ||
            current.stable_key >
                static_cast<std::uint64_t>(
                    std::numeric_limits<std::int64_t>::max()) ||
            current.item_id.empty() || current.room_id.empty() ||
            (current.scale_x != -1 && current.scale_x != 1) ||
            (current.scale_y != -1 && current.scale_y != 1) ||
            !runtime_keys.insert(current.stable_key).second) {
            return {ErrorCode::SnapshotInvalid,
                    "runtime furniture identity is incomplete or ambiguous"};
        }
        const auto saved = saved_by_key.find(current.stable_key);
        if (saved == saved_by_key.end()) {
            return {ErrorCode::SnapshotInvalid,
                    "runtime furniture is missing from the selected save"};
        }
        if (saved->second->item_id != current.item_id) {
            return {ErrorCode::SnapshotInvalid,
                    "runtime furniture item identity changed"};
        }
        saved->second->room_id = current.room_id;
        saved->second->position_x = current.position_x;
        saved->second->position_y = current.position_y;
        saved->second->scale_x = current.scale_x;
        saved->second->scale_y = current.scale_y;
    }

    const bool missing_live_placement = std::ranges::any_of(
        furniture,
        [&runtime_keys](const auto& placement) {
            return !placement.room_id.empty() &&
                !runtime_keys.contains(
                    static_cast<std::uint64_t>(placement.instance_id));
        });
    return missing_live_placement
        ? Result<void>{ErrorCode::SnapshotInvalid,
                       "current placed furniture coverage is incomplete"}
        : Result<void>{};
}

}  // namespace autocattery::ui
