#pragma once

#include <filesystem>
#include <string_view>

#include "auto_cattery/error.hpp"

namespace autocattery::save_safety {

[[nodiscard]] bool IsSafeOperationId(std::string_view value) noexcept;
[[nodiscard]] Result<std::filesystem::path> ResolveContainedExisting(
    const std::filesystem::path& root,
    std::string_view child_name);

}  // namespace autocattery::save_safety
