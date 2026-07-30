#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include "auto_cattery/error.hpp"

namespace autocattery::save_safety {

[[nodiscard]] Result<std::string> Sha256File(
    const std::filesystem::path& path);
[[nodiscard]] Result<std::string> Sha256Text(std::string_view text);

}  // namespace autocattery::save_safety
