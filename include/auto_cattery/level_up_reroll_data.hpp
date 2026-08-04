#pragma once

#include <cstddef>
#include <filesystem>

#include "auto_cattery/error.hpp"

namespace autocattery {

[[nodiscard]] Result<std::filesystem::path> ResolveAutoCatteryDataRoot(
    const std::filesystem::path& mewtator_config);

[[nodiscard]] Result<void> WriteLevelUpRerollData(
    const std::filesystem::path& data_mod_root,
    std::size_t reroll_count);

}  // namespace autocattery
