#pragma once

#include <cstdint>
#include <filesystem>

#include "auto_cattery/error.hpp"
#include "mew_ui_furniture_move_probe.h"

namespace autocattery::ui {

struct FurnitureMoveProbeReport {
    std::uint64_t scene_generation{};
    AcMewFurnitureMoveDiff furniture_ui{};
    AcMewFurnitureMoveDiff house_scene{};
};

[[nodiscard]] Result<std::filesystem::path> WriteFurnitureMoveProbeReport(
    const std::filesystem::path& diagnostics_root,
    const FurnitureMoveProbeReport& report);

}  // namespace autocattery::ui
