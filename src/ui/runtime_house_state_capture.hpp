#pragma once

#include "runtime_house_state.hpp"

namespace autocattery::ui {

[[nodiscard]] Result<RuntimeHouseState> CaptureRuntimeHouseState(
    void* house_scene_manager);

}  // namespace autocattery::ui
