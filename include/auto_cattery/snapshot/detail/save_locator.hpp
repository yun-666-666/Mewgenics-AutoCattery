#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace autocattery::snapshot::detail {

std::optional<std::filesystem::path> FindMostRecentSave(
    const std::filesystem::path& configured_root,
    std::string& error);

}  // namespace autocattery::snapshot::detail
