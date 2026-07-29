#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "auto_cattery/snapshot/domain.hpp"

namespace autocattery::scoring {

inline constexpr char kCombatAlgorithmVersion[] = "known-save-fields-v1";

struct CombatScoringConfig {
    std::uint32_t version{1};
    std::size_t recommended_count{8};
    double minimum_score{};
    std::size_t minimum_known_stats{snapshot::kStatCount};
    bool exclude_kittens{true};
    bool exclude_injured{};
    bool require_confirmed_eligibility{true};
    std::array<double, snapshot::kStatCount> stat_weights{
        1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0
    };
    double missing_stat_penalty{};
    double active_ability_default_weight{};
    double passive_default_weight{};
    double disorder_default_penalty{};
    double injury_penalty{};
    std::unordered_map<std::string, double> active_ability_overrides;
    std::unordered_map<std::string, double> passive_overrides;
    std::unordered_map<std::string, double> disorder_overrides;
};

struct ScoreComponent {
    std::string key;
    double raw_value{};
    double weight{};
    double contribution{};
    std::string explanation;
};

struct CombatScoreResult {
    snapshot::CatId cat_id{};
    bool eligible{};
    double score{};
    double confidence{};
    std::int64_t total_stat_sum{};
    std::vector<std::string> exclusion_reasons;
    std::vector<std::string> limitations;
    std::vector<ScoreComponent> components;
};

struct CombatRanking {
    std::uint64_t source_snapshot_id{};
    std::int64_t game_day{};
    bool game_day_available{};
    std::string algorithm_version;
    std::vector<CombatScoreResult> ranked;
    std::vector<snapshot::CatId> recommended_cat_ids;
};

}  // namespace autocattery::scoring
