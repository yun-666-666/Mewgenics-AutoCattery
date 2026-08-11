#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "auto_cattery/error.hpp"
#include "mew_ui_furniture_move_probe.h"

namespace autocattery::ui {

struct FurnitureProbePiece {
    std::uint64_t stable_key{};
    std::string item;
    std::string room;
    std::int32_t saved_x{};
    std::int32_t saved_y{};
    bool grid_present{};
};

struct FurnitureProbeSceneDelta {
    std::size_t before_count{};
    std::size_t after_count{};
    bool before_complete{};
    bool after_complete{};
    std::vector<FurnitureProbePiece> appeared;
    std::vector<FurnitureProbePiece> disappeared;
    std::vector<FurnitureProbePiece> changed;
};

struct FurnitureMoveProbeReport {
    std::uint64_t scene_generation{};
    AcMewFurnitureMoveDiff furniture_ui{};
    AcMewFurnitureMoveDiff house_inventory{};
    AcMewFurnitureMoveDiff furniture_editor{};
    AcMewFurnitureMoveDiff furniture_click_handler{};
    AcMewFurnitureMoveDiff house_scene{};
    FurnitureProbeSceneDelta scene_furniture;
};

[[nodiscard]] Result<std::filesystem::path> WriteFurnitureMoveProbeReport(
    const std::filesystem::path& diagnostics_root,
    const FurnitureMoveProbeReport& report);

}  // namespace autocattery::ui
