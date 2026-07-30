#pragma once

#include <filesystem>
#include <string_view>

#include "auto_cattery/error.hpp"

namespace autocattery::save_safety {

[[nodiscard]] bool IsSafeOperationId(std::string_view value) noexcept;
[[nodiscard]] bool IsSameOrWithin(
    const std::filesystem::path& root,
    const std::filesystem::path& candidate) noexcept;
[[nodiscard]] Result<std::filesystem::path> ResolveContainedExisting(
    const std::filesystem::path& root,
    std::string_view child_name);
[[nodiscard]] Result<std::filesystem::path> ResolveContainedExistingFile(
    const std::filesystem::path& root,
    const std::filesystem::path& candidate);

}  // namespace autocattery::save_safety
