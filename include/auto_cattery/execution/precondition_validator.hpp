#pragma once

#include <optional>
#include <span>
#include <string>

#include "auto_cattery/execution/domain.hpp"
#include "auto_cattery/protection/domain.hpp"
#include "auto_cattery/room_planning/domain.hpp"

namespace autocattery::execution {

enum class ApprovalDisposition {
    Approved,
    CancelAndRepreview,
    Unsupported
};

struct ApprovalRequest {
    OperationId operation_id;
    const snapshot::HouseSnapshot& snapshot;
    const classification::ClassificationPlan& classification;
    std::span<const protection::ProtectionDecision> protections;
    const room_planning::RoomPlan& room_plan;
    protection::ProtectionDigest preview_protection_digest;
    protection::ProtectionDigest current_protection_digest;
    std::uint64_t expected_scene_generation{};
    std::uint64_t current_scene_generation{};
    std::optional<std::int64_t> current_game_day;
    std::string save_identity;
    std::string game_build_identity;
    bool approve_culls{};
};

struct ApprovalResult {
    ApprovalDisposition disposition{ApprovalDisposition::Unsupported};
    std::optional<ApprovedExecutionPlan> plan;
    std::optional<ExecutionAuthorization> authorization;
    std::string reason;
};

class PreconditionValidator final {
public:
    [[nodiscard]] ApprovalResult Validate(
        const ApprovalRequest& request) const;
};

[[nodiscard]] bool AuthorizationMatches(
    const ApprovedExecutionPlan& plan,
    const ExecutionAuthorization& authorization) noexcept;

}  // namespace autocattery::execution
