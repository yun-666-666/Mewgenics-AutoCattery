#pragma once

#include <filesystem>

#include "auto_cattery/error.hpp"
#include "auto_cattery/save_safety/backup_manifest.hpp"

namespace autocattery::save_safety {

[[nodiscard]] Result<void> WriteBackupManifest(
    const std::filesystem::path& path,
    const BackupManifest& manifest);
[[nodiscard]] Result<BackupManifest> ReadBackupManifest(
    const std::filesystem::path& path);
[[nodiscard]] std::string UtcTimestamp();

}  // namespace autocattery::save_safety
