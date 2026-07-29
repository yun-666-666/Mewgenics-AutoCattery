#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "auto_cattery/snapshot/domain.hpp"

namespace autocattery::classification {

inline constexpr char kClassificationAlgorithmVersion[] =
    "safe-preview-classification-v1";

enum class CatRole {
    CombatRecommended,
    BreedingCore,
    BreedingReserve,
    GeneralReserve,
    CullCandidate,
    ProtectedUnmanaged,
    Ineligible
};

struct CullSafetyFacts {
    snapshot::TriState protected_from_cull{snapshot::TriState::Unknown};
    snapshot::TriState special_state_present{snapshot::TriState::Unknown};
};

using CullSafetyFactsByCat =
    std::unordered_map<snapshot::CatId, CullSafetyFacts>;

struct ClassificationConfig {
    std::uint32_t version{1};
    bool combat_priority_over_breeding{};
    std::size_t minimum_combat_pool{8};
    std::size_t minimum_breeding_pool{8};
    std::size_t minimum_general_reserve{4};
    double never_cull_if_data_confidence_below{0.85};
};

struct CatDecision {
    snapshot::CatId cat_id{};
    CatRole primary_role{CatRole::GeneralReserve};
    double combat_score{};
    double breeding_score{};
    double confidence{};
    bool combat_recommended{};
    bool combat_pool_protected{};
    bool breeding_core{};
    bool breeding_reserve{};
    bool breeding_pool_protected{};
    bool preview_cull_candidate{};
    bool destructive_action_allowed{};
    std::vector<std::string> reasons;
};

struct ClassificationPlan {
    std::uint64_t source_snapshot_id{};
    std::string algorithm_version;
    std::vector<CatDecision> decisions;
    std::vector<snapshot::CatId> quality_cull_candidates;
    std::vector<snapshot::CatId> capacity_relief_candidates;
    std::vector<std::string> global_warnings;
};

}  // namespace autocattery::classification
