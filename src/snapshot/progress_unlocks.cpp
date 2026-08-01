#include "auto_cattery/snapshot/detail/progress_unlocks.hpp"

#include <algorithm>
#include <string_view>

namespace autocattery::snapshot::detail {
namespace {

bool Contains(
    std::span<const std::byte> bytes,
    std::string_view token) {
    const auto* begin = reinterpret_cast<const char*>(bytes.data());
    const std::string_view text(begin, bytes.size());
    return text.find(token) != std::string_view::npos;
}

}  // namespace

ProgressUnlocks ParseProgressUnlocks(
    std::span<const std::byte> npc_progress) {
    return {
        // The fixed seven-stat block exists in every current-build cat blob,
        // including saves made before Tink exposes it in the game UI.
        .base_stats = true,
        .sexuality = Contains(npc_progress, "tink_sexuality"),
        .pedigree = Contains(npc_progress, "tink_inbreeding") &&
            Contains(npc_progress, "tink_relationships")
    };
}

}  // namespace autocattery::snapshot::detail
