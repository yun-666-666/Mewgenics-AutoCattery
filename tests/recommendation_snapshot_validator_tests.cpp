#include "auto_cattery/recommendation/snapshot_validator.hpp"

#include "test_support.hpp"

namespace autocattery::tests {

void RunRecommendationSnapshotValidatorTests() {
    workflow::RecommendationSnapshot historical;
    historical.game_day = 12;
    historical.config_digest = "config-a";
    historical.combat_algorithm_version = "combat-v1";
    historical.recommended = {
        {101, 1, 91.5, 1.0},
        {202, 2, 80.0, 0.9}
    };

    recommendation::SnapshotCompatibilityContext context;
    context.current_game_day = 13;
    context.config_digest = "config-a";
    context.combat_algorithm_version = "combat-v1";
    context.snapshot_build_identity_available = true;
    context.snapshot_save_identity_available = true;
    context.current_candidates_verified = true;
    context.confirmed_candidate_ids = {101};
    const auto usable =
        recommendation::ValidateHistoricalSnapshot(historical, context);
    AC_CHECK(
        usable.disposition ==
        recommendation::HistoricalSnapshotDisposition::Usable);
    AC_CHECK(usable.matching_entries.size() == 1);

    context.current_game_day.reset();
    AC_CHECK(
        recommendation::ValidateHistoricalSnapshot(historical, context)
            .disposition ==
        recommendation::HistoricalSnapshotDisposition::RecomputeRequired);
    context.current_game_day = 14;
    AC_CHECK(
        recommendation::ValidateHistoricalSnapshot(historical, context)
            .disposition ==
        recommendation::HistoricalSnapshotDisposition::RecomputeRequired);
    context.current_game_day = 13;
    context.config_digest = "config-b";
    AC_CHECK(
        recommendation::ValidateHistoricalSnapshot(historical, context)
            .disposition ==
        recommendation::HistoricalSnapshotDisposition::RecomputeRequired);
    context.config_digest = "config-a";
    context.combat_algorithm_version = "combat-v2";
    AC_CHECK(
        recommendation::ValidateHistoricalSnapshot(historical, context)
            .disposition ==
        recommendation::HistoricalSnapshotDisposition::RecomputeRequired);
    context.combat_algorithm_version = "combat-v1";
    context.snapshot_build_identity_available = false;
    context.snapshot_save_identity_available = false;
    const auto actual_schema_limit =
        recommendation::ValidateHistoricalSnapshot(historical, context);
    AC_CHECK(actual_schema_limit.limitations.size() == 2);
}

}  // namespace autocattery::tests
