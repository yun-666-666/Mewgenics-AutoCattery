#include "runtime_house_state.hpp"

#include <algorithm>
#include <limits>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace autocattery::ui {

RuntimeFurnitureRoomSignature BuildRuntimeFurnitureRoomSignature(
    const RuntimeFurnitureState& runtime,
    const snapshot::RoomId& room_id) {
    RuntimeFurnitureRoomSignature signature{.room_id = room_id};
    for (const auto& placement : runtime.placements) {
        if (placement.room_id == room_id) {
            signature.placements.push_back(placement);
        }
    }
    std::ranges::sort(
        signature.placements,
        [](const auto& left, const auto& right) {
            return std::tie(
                       left.stable_key,
                       left.item_id,
                       left.position_x,
                       left.position_y,
                       left.scale_x,
                       left.scale_y) <
                std::tie(
                       right.stable_key,
                       right.item_id,
                       right.position_x,
                       right.position_y,
                       right.scale_x,
                       right.scale_y);
        });
    return signature;
}

void ReconcileLockedFurnitureRooms(
    std::vector<snapshot::RoomId>& locked_room_ids,
    std::vector<RuntimeFurnitureRoomSignature>& locked_room_signatures,
    const RuntimeFurnitureState& runtime,
    std::vector<snapshot::RoomId>* invalidated_room_ids) {
    // Room-by-room signatures cannot distinguish an intentionally cleared
    // whole house from rooms that were already empty when they were locked.
    // Once the live scene has no placed furniture, every old completion lock
    // is stale: retaining an empty-room signature would permanently exclude
    // that room from the next sealed whole-house blueprint.
    if (runtime.placements.empty()) {
        if (invalidated_room_ids) {
            invalidated_room_ids->insert(
                invalidated_room_ids->end(),
                locked_room_ids.begin(),
                locked_room_ids.end());
        }
        locked_room_ids.clear();
        locked_room_signatures.clear();
        return;
    }
    std::vector<snapshot::RoomId> retained_ids;
    std::vector<RuntimeFurnitureRoomSignature> retained_signatures;
    retained_ids.reserve(locked_room_ids.size());
    retained_signatures.reserve(locked_room_ids.size());
    for (const auto& room_id : locked_room_ids) {
        const auto expected = std::ranges::find(
            locked_room_signatures,
            room_id,
            &RuntimeFurnitureRoomSignature::room_id);
        const auto current = BuildRuntimeFurnitureRoomSignature(
            runtime, room_id);
        if (expected == locked_room_signatures.end() ||
            *expected != current) {
            if (invalidated_room_ids) {
                invalidated_room_ids->push_back(room_id);
            }
            continue;
        }
        retained_ids.push_back(room_id);
        retained_signatures.push_back(std::move(current));
    }
    locked_room_ids = std::move(retained_ids);
    locked_room_signatures = std::move(retained_signatures);
}

void LockFurnitureRoom(
    std::vector<snapshot::RoomId>& locked_room_ids,
    std::vector<RuntimeFurnitureRoomSignature>& locked_room_signatures,
    const RuntimeFurnitureState& runtime,
    const snapshot::RoomId& room_id) {
    if (room_id.empty()) {
        return;
    }
    const auto signature = BuildRuntimeFurnitureRoomSignature(
        runtime, room_id);
    const auto existing = std::ranges::find(locked_room_ids, room_id);
    if (existing == locked_room_ids.end()) {
        locked_room_ids.push_back(room_id);
    }
    const auto saved = std::ranges::find(
        locked_room_signatures,
        room_id,
        &RuntimeFurnitureRoomSignature::room_id);
    if (saved == locked_room_signatures.end()) {
        locked_room_signatures.push_back(signature);
    } else {
        *saved = signature;
    }
}

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

    // The save can lag behind the live House after an Auto Place batch.  In
    // particular, a warehouse replacement becomes the live placed stable key
    // while the replaced stable key is still recorded as placed until the
    // player saves.  The native enumeration is complete at this point, so it
    // is the authority for which saved records are currently in a room.
    for (auto& placement : furniture) {
        placement.room_id.clear();
    }
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

    return {};
}

}  // namespace autocattery::ui
