#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "auto_cattery/error.hpp"
#include "auto_cattery/snapshot/domain.hpp"

namespace autocattery::ui {

using RuntimePointer = std::uintptr_t;

struct RuntimeCatRoomState {
    snapshot::CatId cat_id{};
    RuntimePointer component{};
    RuntimePointer room{};
};

struct RuntimeRoomEvidence {
    RuntimePointer room{};
    std::vector<snapshot::RoomId> detected_ids;
};

struct RuntimeHouseState {
    std::size_t available_room_count{};
    std::vector<RuntimeCatRoomState> cats;
    std::vector<RuntimeRoomEvidence> rooms;
};

[[nodiscard]] Result<std::unordered_map<snapshot::RoomId, RuntimePointer>>
ResolveRuntimeRoomPointers(
    const snapshot::HouseSnapshot& snapshot,
    const RuntimeHouseState& runtime);

[[nodiscard]] Result<void> OverlayRuntimeHouseState(
    snapshot::HouseSnapshot& snapshot,
    const RuntimeHouseState& runtime);

[[nodiscard]] Result<void> OverlayRuntimeHouseState(
    snapshot::HouseSnapshot& snapshot,
    const RuntimeHouseState& runtime,
    const std::unordered_map<snapshot::RoomId, RuntimePointer>& room_pointers);

[[nodiscard]] bool RuntimeHouseStateMatches(
    const snapshot::HouseSnapshot& snapshot,
    const RuntimeHouseState& runtime);

}  // namespace autocattery::ui
