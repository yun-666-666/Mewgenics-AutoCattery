#include "runtime_house_state.hpp"

#include <algorithm>
#include <unordered_map>

namespace autocattery::ui {

Result<void> OverlayRuntimeHouseState(
    snapshot::HouseSnapshot& snapshot,
    const RuntimeHouseState& runtime) {
    const auto room_pointers =
        ResolveRuntimeRoomPointers(snapshot, runtime);
    if (!room_pointers) {
        return {room_pointers.code, room_pointers.message};
    }
    std::unordered_map<RuntimePointer, snapshot::RoomId> room_ids;
    for (const auto& [id, pointer] : room_pointers.value) {
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

}  // namespace autocattery::ui
