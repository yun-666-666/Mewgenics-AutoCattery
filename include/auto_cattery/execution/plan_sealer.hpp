#pragma once

#include <span>
#include <string>

#include "auto_cattery/classification/domain.hpp"
#include "auto_cattery/execution/domain.hpp"
#include "auto_cattery/protection/domain.hpp"
#include "auto_cattery/room_planning/domain.hpp"

namespace autocattery::execution {

[[nodiscard]] std::string DigestSnapshotContent(
    const snapshot::HouseSnapshot& snapshot);

[[nodiscard]] std::string DigestClassification(
    const classification::ClassificationPlan& classification);

[[nodiscard]] CanonicalPlanDigest DigestRoomPlan(
    const room_planning::RoomPlan& plan);

[[nodiscard]] std::uint64_t BuildAuthorizationSeal(
    const OperationId& operation_id,
    const OperationPrecondition& precondition) noexcept;

}  // namespace autocattery::execution
