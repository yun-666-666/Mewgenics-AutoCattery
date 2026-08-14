#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <ranges>

#include "auto_cattery/furniture_planning/config.hpp"
#include "auto_cattery/room_planning/domain.hpp"
#include "auto_cattery/snapshot/detail/furniture_attributes.hpp"

namespace autocattery::furniture_planning {

using FurniturePurposeRank = std::array<double, 8>;

namespace purpose_policy_detail {

struct Constraint {
    double value{};
    double target{};
    bool maximum{};
};

inline const FurniturePurposeTargets& TargetsFor(
    room_planning::RoomRole role,
    const FurniturePlacementConfig& config) noexcept {
    switch (role) {
        case room_planning::RoomRole::Breeding:
            return config.breeding;
        case room_planning::RoomRole::CombatStaging:
            return config.combat;
        case room_planning::RoomRole::Kitten:
        case room_planning::RoomRole::Recovery:
            return config.kitten_recovery;
        case room_planning::RoomRole::MutationLab:
            return config.mutation;
        default:
            return config.general;
    }
}

inline std::array<Constraint, 4> Constraints(
    room_planning::RoomRole role,
    const snapshot::RoomAttributes& attributes,
    const FurniturePlacementConfig& config) noexcept {
    const auto& targets = TargetsFor(role, config);
    const bool combat = role == room_planning::RoomRole::CombatStaging;
    return {{
        {attributes.comfort, targets.comfort_per_resident, combat},
        {attributes.stimulation, targets.stimulation_per_resident, combat},
        {attributes.health, targets.health_per_resident, false},
        {attributes.mutation, targets.mutation_per_resident, false},
    }};
}

inline bool Satisfied(const Constraint& constraint) noexcept {
    return constraint.maximum
        ? constraint.value <= constraint.target
        : constraint.value >= constraint.target;
}

inline double Deficit(const Constraint& constraint) noexcept {
    return constraint.maximum
        ? std::max(0.0, constraint.value - constraint.target)
        : std::max(0.0, constraint.target - constraint.value);
}

inline double DirectionalUtility(const Constraint& constraint) noexcept {
    return constraint.maximum ? -constraint.value : constraint.value;
}

inline std::array<std::size_t, 2> SafetyIndices(
    room_planning::RoomRole role) noexcept {
    switch (role) {
        case room_planning::RoomRole::Breeding:
            return {0U, 1U};
        case room_planning::RoomRole::CombatStaging:
        case room_planning::RoomRole::Kitten:
        case room_planning::RoomRole::Recovery:
        case room_planning::RoomRole::MutationLab:
            return {0U, 2U};
        default:
            return {0U, 2U};
    }
}

}  // namespace purpose_policy_detail

inline FurniturePurposeRank RankFurniturePurpose(
    room_planning::RoomRole role,
    const snapshot::RoomAttributes& attributes,
    std::size_t expected_resident_count,
    const FurniturePlacementConfig& config) noexcept {
    // Kept in this public signature for existing callers. Furniture targets
    // are whole-room totals, so resident count must not affect the rank.
    (void)expected_resident_count;
    const auto constraints = purpose_policy_detail::Constraints(
        role, attributes, config);
    double satisfied_count{};
    double deficit_sum{};
    double directional_utility_sum{};
    for (const auto& constraint : constraints) {
        satisfied_count += purpose_policy_detail::Satisfied(constraint)
            ? 1.0 : 0.0;
        deficit_sum += purpose_policy_detail::Deficit(constraint);
        directional_utility_sum +=
            purpose_policy_detail::DirectionalUtility(constraint);
    }
    const auto safety = purpose_policy_detail::SafetyIndices(role);
    const auto safety_count =
        (purpose_policy_detail::Satisfied(constraints[safety[0]]) ? 1.0 : 0.0) +
        (purpose_policy_detail::Satisfied(constraints[safety[1]]) ? 1.0 : 0.0);
    const bool all_satisfied = satisfied_count == constraints.size();
    return {
        safety_count,
        satisfied_count,
        -deficit_sum,
        all_satisfied ? 1.0 : 0.0,
        directional_utility_sum,
        purpose_policy_detail::DirectionalUtility(constraints[0]),
        purpose_policy_detail::DirectionalUtility(constraints[1]),
        purpose_policy_detail::DirectionalUtility(constraints[3])};
}

inline FurniturePurposeRank RankFurniturePurpose(
    const room_planning::RoomPurposeAssignment* purpose,
    const snapshot::RoomAttributes& attributes,
    const FurniturePlacementConfig& config) noexcept {
    return RankFurniturePurpose(
        purpose ? purpose->role : room_planning::RoomRole::General,
        attributes,
        purpose ? purpose->expected_resident_count : 0U,
        config);
}

inline bool FurniturePurposeNeedsMore(
    const room_planning::RoomPurposeAssignment* purpose,
    const snapshot::RoomAttributes& attributes,
    const FurniturePlacementConfig& config) noexcept {
    if (!purpose) {
        return true;
    }
    const auto constraints = purpose_policy_detail::Constraints(
        purpose->role,
        attributes,
        config);
    return std::ranges::any_of(
        constraints,
        [](const auto& constraint) {
            return !purpose_policy_detail::Satisfied(constraint);
        });
}

}  // namespace autocattery::furniture_planning
