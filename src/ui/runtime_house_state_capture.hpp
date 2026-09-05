#pragma once

#include <span>

#include "runtime_house_state.hpp"

namespace autocattery::ui {

[[nodiscard]] std::size_t InferAvailableRoomCount(
    std::size_t native_room_count,
    std::span<const RuntimeCatRoomState> cats);

[[nodiscard]] Result<RuntimeHouseState> CaptureRuntimeHouseState(
    void* house_scene_manager);

}  // namespace autocattery::ui
