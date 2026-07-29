#include "auto_cattery/snapshot/save_snapshot_adapter.hpp"
#include "auto_cattery/scoring/combat_ranker.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <vector>

int wmain(int argument_count, wchar_t** arguments) {
    const std::filesystem::path configured_save =
        argument_count > 1 ? arguments[1] : L"";
    autocattery::snapshot::SaveSnapshotAdapter adapter(configured_save);
    const auto result = adapter.CaptureHouseSnapshot(1);
    if (!result) {
        std::cerr << "snapshot failed: " << result.message << '\n';
        return 1;
    }

    const auto& snapshot = result.value;
    const auto assigned = std::count_if(
        snapshot.cats.begin(),
        snapshot.cats.end(),
        [](const auto& cat) { return cat.room_id.has_value(); });
    const auto adventure = std::count_if(
        snapshot.cats.begin(),
        snapshot.cats.end(),
        [](const auto& cat) { return cat.in_adventure_box; });
    const auto validation = autocattery::snapshot::Validate(snapshot);
    const auto repeated = adapter.CaptureHouseSnapshot(1);
    std::vector<autocattery::snapshot::CatId> first_ids;
    std::vector<autocattery::snapshot::CatId> repeated_ids;
    first_ids.reserve(snapshot.cats.size());
    if (repeated) {
        repeated_ids.reserve(repeated.value.cats.size());
    }
    for (const auto& cat : snapshot.cats) {
        first_ids.push_back(cat.id);
    }
    if (repeated) {
        for (const auto& cat : repeated.value.cats) {
            repeated_ids.push_back(cat.id);
        }
    }
    const bool stable_ids =
        repeated && first_ids == repeated_ids;
    autocattery::scoring::CombatScoringConfig scoring_config;
    const auto ranking =
        autocattery::scoring::RankCombatCats(snapshot, scoring_config);
    const auto repeated_ranking = repeated
        ? autocattery::scoring::RankCombatCats(
            repeated.value,
            scoring_config)
        : autocattery::Result<autocattery::scoring::CombatRanking>{
            {},
            autocattery::ErrorCode::SnapshotInvalid,
            "repeated snapshot unavailable"
        };
    std::vector<autocattery::snapshot::CatId> ranked_ids;
    std::vector<autocattery::snapshot::CatId> repeated_ranked_ids;
    if (ranking) {
        for (const auto& score : ranking.value.ranked) {
            ranked_ids.push_back(score.cat_id);
        }
    }
    if (repeated_ranking) {
        for (const auto& score : repeated_ranking.value.ranked) {
            repeated_ranked_ids.push_back(score.cat_id);
        }
    }
    const bool stable_ranking =
        ranking && repeated_ranking && ranked_ids == repeated_ranked_ids;
    std::cout
        << "house_cats=" << snapshot.cats.size()
        << " rooms=" << snapshot.rooms.size()
        << " assigned=" << assigned
        << " adventure=" << adventure
        << " day=";
    if (snapshot.game_day) {
        std::cout << *snapshot.game_day;
    } else {
        std::cout << "unavailable";
    }
    std::cout
        << " warnings=" << validation.WarningCount()
        << " errors=" << validation.ErrorCount()
        << " stable_ids=" << (stable_ids ? 1 : 0)
        << " ranked=" << (ranking ? ranking.value.ranked.size() : 0)
        << " recommended="
        << (ranking ? ranking.value.recommended_cat_ids.size() : 0)
        << " stable_ranking=" << (stable_ranking ? 1 : 0)
        << '\n';
    return validation.Valid() && stable_ids && stable_ranking ? 0 : 2;
}
