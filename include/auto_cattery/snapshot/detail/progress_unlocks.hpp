#pragma once

#include <cstddef>
#include <span>

namespace autocattery::snapshot::detail {

struct ProgressUnlocks {
    bool base_stats{};
    bool sexuality{};
    bool pedigree{};
};

[[nodiscard]] ProgressUnlocks ParseProgressUnlocks(
    std::span<const std::byte> npc_progress);

}  // namespace autocattery::snapshot::detail
