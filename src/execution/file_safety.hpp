#pragma once

#include <filesystem>
#include <string_view>

namespace autocattery::execution::detail {

bool IsSafeToken(std::string_view value) noexcept;
bool PrepareContainedDirectory(
    const std::filesystem::path& root,
    const std::filesystem::path& child,
    std::filesystem::path& canonical_root,
    std::filesystem::path& canonical_child);
bool AtomicPublish(
    const std::filesystem::path& temporary,
    const std::filesystem::path& destination,
    bool replace);

}  // namespace autocattery::execution::detail
