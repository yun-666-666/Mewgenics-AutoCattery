#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "auto_cattery/classification/domain.hpp"
#include "auto_cattery/protection/domain.hpp"
#include "auto_cattery/snapshot/domain.hpp"

namespace autocattery::room_planning {

inline constexpr char kRoomPlanningAlgorithmVersion[] =
    "capacity-aware-room-planner-v1";
inline constexpr char kBalancedMoveOnlyAlgorithmVersion[] =
    "current-build-purpose-aware-room-planning-v8";

enum class CapabilityState {
    Unknown,
    No,
    Yes
};

enum class RoomRole {
    Unknown,
    General,
    Breeding,
    CombatStaging,
    Kitten,
    Special,
    Unavailable
};

struct RoomCapability {
    snapshot::RoomId room_id;
    RoomRole confirmed_role{RoomRole::Unknown};
    std::optional<std::size_t> confirmed_hard_capacity;
    CapabilityState special_room{CapabilityState::Unknown};
    CapabilityState player_locked{CapabilityState::Unknown};
    CapabilityState forced_residents_present{CapabilityState::Unknown};
    CapabilityState can_receive_residents{CapabilityState::Unknown};
    CapabilityState can_release_residents{CapabilityState::Unknown};
    CapabilityState native_capacity_gate{CapabilityState::Unknown};
};

struct RoomPlanningConfig {
    std::uint32_t version{1};
    std::size_t default_soft_capacity{4};
    bool allow_soft_overflow{true};
    std::size_t max_soft_overflow_per_room{2};
    bool never_exceed_known_hard_capacity{true};
    bool prefer_single_combat_staging_room{true};
    bool keep_breeding_pairs_together{true};
    bool avoid_inbreeding_pairs{true};
    bool keep_kittens_separate_when_possible{true};
    bool allow_partial_plan{true};
};

struct PlannedMove {
    snapshot::CatId cat_id{};
    snapshot::RoomId from_room;
    snapshot::RoomId to_room;
    std::string reason;
    int priority{};
    bool executable{};

    bool operator==(const PlannedMove&) const = default;
};

struct UnplacedCat {
    snapshot::CatId cat_id{};
    std::string reason;

    bool operator==(const UnplacedCat&) const = default;
};

struct CapacityReliefSuggestion {
    snapshot::CatId cat_id{};
    std::size_t candidate_order{};
    std::string reason;
    bool executable{};

    bool operator==(const CapacityReliefSuggestion&) const = default;
};

enum class PlanDisposition {
    Complete,
    Partial,
    Invalid,
    CancelAndRepreview
};

struct RoomPlan {
    std::uint64_t source_snapshot_id{};
    std::string algorithm_version{kRoomPlanningAlgorithmVersion};
    std::vector<PlannedMove> moves;
    std::vector<UnplacedCat> unplaced_cats;
    std::vector<CapacityReliefSuggestion> capacity_relief_suggestions;
    std::vector<std::string> limitations;
    std::vector<std::string> warnings;
    std::vector<std::string> validation_errors;
    std::size_t minimum_capacity_relief_required{};
    PlanDisposition disposition{PlanDisposition::Invalid};
    bool fully_satisfied{};
    bool move_execution_allowed{};
    bool cull_execution_allowed{};

    bool operator==(const RoomPlan&) const = default;
};

struct RoomPlanningInput {
    const snapshot::HouseSnapshot& snapshot;
    const classification::ClassificationPlan& classification;
    std::span<const protection::ProtectionDecision> protections;
    std::span<const RoomCapability> room_capabilities;
    protection::ProtectionDigest preview_protection_digest;
    protection::ProtectionDigest current_protection_digest;
};

}  // namespace autocattery::room_planning
