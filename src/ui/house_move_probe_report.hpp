#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include "auto_cattery/error.hpp"
#include "mew_ui_house_move_probe.h"

namespace autocattery::ui {

// Contains bounded summaries only; the writer deliberately omits identities.
struct HouseMoveProbeReport {
    std::uint64_t scene_generation{};
    std::vector<AcMewHouseMoveDiff> differences;
};

[[nodiscard]] Result<std::filesystem::path> WriteHouseMoveProbeReport(
    const std::filesystem::path& diagnostics_root,
    const HouseMoveProbeReport& report);

}  // namespace autocattery::ui
