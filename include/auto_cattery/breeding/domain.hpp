#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "auto_cattery/scoring/domain.hpp"
#include "auto_cattery/snapshot/domain.hpp"

namespace autocattery::breeding {

inline constexpr char kBreedingAlgorithmVersion[] =
    "configured-pair-stat-weights-libido-v6";

enum class BreedingStage {
    Foundation,
    BaseAllSeven,
    StableAllSeven
};

struct BreedingPairScore {
    snapshot::CatId cat_a_id{};
    snapshot::CatId cat_b_id{};
    bool eligible{};
    double score{};
    std::size_t covered_seven_stats{};
    std::size_t jointly_stable_seven_stats{};
    std::optional<double> offspring_inbreeding_coefficient;
    bool stable_all_seven{};
    double trait_score{};
    std::vector<std::string> exclusion_reasons;
};

struct BreedingScoringConfig {
    std::uint32_t version{1};
    std::size_t core_breeders{4};
    std::size_t reserve_breeders{4};
    double minimum_score{};
    std::size_t minimum_known_stats{snapshot::kStatCount};
    bool require_confirmed_eligibility{true};
    std::array<double, snapshot::kStatCount> stat_weights{
        1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0
    };
    double missing_stat_penalty{};
    double active_ability_default_weight{1.0};
    double passive_default_weight{1.0};
    double disorder_default_penalty{1.0};
    double mutation_default_weight{1.0};
    double birth_defect_default_penalty{1.0};
    std::unordered_map<std::string, double> active_ability_overrides;
    std::unordered_map<std::string, double> passive_overrides;
    std::unordered_map<std::string, double> disorder_overrides;
    std::unordered_map<std::string, double> mutation_overrides;
    std::unordered_map<std::string, double> birth_defect_overrides;
};

struct BreedingScoreResult {
    snapshot::CatId cat_id{};
    bool eligible{};
    double score{};
    double confidence{};
    std::int64_t heritable_stat_sum{};
    std::vector<std::string> exclusion_reasons;
    std::vector<std::string> limitations;
    std::vector<scoring::ScoreComponent> components;
};

struct BreedingRanking {
    std::uint64_t source_snapshot_id{};
    std::string algorithm_version;
    std::vector<BreedingScoreResult> ranked;
    BreedingStage stage{BreedingStage::Foundation};
    std::vector<BreedingPairScore> ranked_pairs;
    std::optional<BreedingPairScore> recommended_pair;
};

}  // namespace autocattery::breeding
