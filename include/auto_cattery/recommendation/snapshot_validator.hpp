#pragma once

#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "auto_cattery/snapshot/domain.hpp"
#include "auto_cattery/workflow/recommendation_snapshot_writer.hpp"

namespace autocattery::recommendation {

enum class HistoricalSnapshotDisposition {
    Usable,
    RecomputeRequired
};

struct SnapshotCompatibilityContext {
    std::optional<std::int64_t> current_game_day;
    std::string config_digest;
    std::string combat_algorithm_version;
    bool snapshot_build_identity_available{};
    bool snapshot_save_identity_available{};
    bool current_candidates_verified{};
    std::unordered_set<snapshot::CatId> confirmed_candidate_ids;
};

struct SnapshotCompatibility {
    HistoricalSnapshotDisposition disposition{
        HistoricalSnapshotDisposition::RecomputeRequired};
    std::vector<workflow::RecommendationEntry> matching_entries;
    std::vector<std::string> limitations;
};

[[nodiscard]] SnapshotCompatibility ValidateHistoricalSnapshot(
    const workflow::RecommendationSnapshot& historical,
    const SnapshotCompatibilityContext& current);

}  // namespace autocattery::recommendation
