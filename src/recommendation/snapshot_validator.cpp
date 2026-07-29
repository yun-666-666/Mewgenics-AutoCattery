#include "auto_cattery/recommendation/snapshot_validator.hpp"

namespace autocattery::recommendation {

SnapshotCompatibility ValidateHistoricalSnapshot(
    const workflow::RecommendationSnapshot& historical,
    const SnapshotCompatibilityContext& current) {
    SnapshotCompatibility result;
    if (!current.current_game_day) {
        result.limitations.push_back("game-day-unknown");
    } else if (*current.current_game_day != historical.game_day + 1) {
        result.limitations.push_back("recommendation-day-stale");
    }
    if (current.config_digest != historical.config_digest) {
        result.limitations.push_back("combat-config-changed");
    }
    if (current.combat_algorithm_version !=
        historical.combat_algorithm_version) {
        result.limitations.push_back("combat-algorithm-changed");
    }
    if (!current.snapshot_build_identity_available) {
        result.limitations.push_back("snapshot-build-identity-unavailable");
    }
    if (!current.snapshot_save_identity_available) {
        result.limitations.push_back("snapshot-save-identity-unavailable");
    }
    if (!current.current_candidates_verified) {
        result.limitations.push_back("current-candidates-unverified");
    }

    if (result.limitations.empty()) {
        for (const auto& entry : historical.recommended) {
            if (current.confirmed_candidate_ids.contains(entry.cat_id)) {
                result.matching_entries.push_back(entry);
            }
        }
        result.disposition = HistoricalSnapshotDisposition::Usable;
    }
    return result;
}

}  // namespace autocattery::recommendation
