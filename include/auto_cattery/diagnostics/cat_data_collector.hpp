#pragma once

#include <filesystem>

#include "auto_cattery/error.hpp"
#include "auto_cattery/workflow/preview_builder.hpp"

namespace autocattery::diagnostics {

// Writes only game/algorithm data. Player cat names, save names, account data,
// absolute paths, and machine identifiers are intentionally excluded.
[[nodiscard]] Result<std::filesystem::path> WriteCatDataSnapshot(
    const workflow::PreviewBundle& bundle,
    const std::filesystem::path& data_root);

}  // namespace autocattery::diagnostics
