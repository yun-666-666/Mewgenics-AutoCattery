#pragma once

#include <filesystem>

#include "auto_cattery/error.hpp"

namespace autocattery::save_safety {

[[nodiscard]] Result<std::filesystem::path> ResolveIsolatedTestSave(
    const std::filesystem::path& test_root,
    const std::filesystem::path& test_save,
    const std::filesystem::path& game_executable);

}  // namespace autocattery::save_safety
