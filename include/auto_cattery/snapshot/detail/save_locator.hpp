#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace autocattery::snapshot::detail {

std::vector<std::filesystem::path> FindSaveCandidates(
    const std::filesystem::path& configured_root,
    std::string& error);
std::optional<std::filesystem::path> FindMostRecentSave(
    const std::filesystem::path& configured_root,
    std::string& error);

}  // namespace autocattery::snapshot::detail
