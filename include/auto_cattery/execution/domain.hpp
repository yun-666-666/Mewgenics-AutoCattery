#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "auto_cattery/snapshot/domain.hpp"

namespace autocattery::execution {

using OperationId = std::string;

struct CanonicalPlanDigest {
    std::string value;

    bool operator==(const CanonicalPlanDigest&) const = default;
};

struct OperationPrecondition {
    std::uint64_t snapshot_id{};
    std::uint64_t scene_generation{};
    std::optional<std::int64_t> game_day;
    std::string game_build_identity;
    std::string save_identity;
    std::string snapshot_content_digest;
    std::string classification_digest;
    CanonicalPlanDigest plan_digest;
    std::uint64_t protection_digest{};

    bool operator==(const OperationPrecondition&) const = default;
};

struct ApprovedMove {
    snapshot::CatId cat_id{};
    snapshot::RoomId from_room;
    snapshot::RoomId to_room;

    bool operator==(const ApprovedMove&) const = default;
};

struct ApprovedCull {
    snapshot::CatId cat_id{};
    std::size_t candidate_order{};

    bool operator==(const ApprovedCull&) const = default;
};

enum class FailureReason {
    None,
    Unsupported,
    CancelAndRepreview,
    AlreadyExecuted,
    Busy,
    BackupFailed,
    RecoveryPackageFailed,
    JournalFailed,
    PreconditionsChanged,
    MoveFailed,
    MoveVerificationFailed,
    CullFailed,
    CullVerificationFailed,
    RollbackFailed
};

enum class JournalStatus {
    Prepared,
    Committed,
    RolledBack,
    ManualRecoveryRequired
};

struct UndoRecord {
    std::size_t operation_index{};
    std::optional<snapshot::RoomId> old_room;
    std::optional<snapshot::RoomId> new_room;
    bool was_culled{};
};

struct OperationJournalEntry {
    OperationId operation_id;
    OperationPrecondition precondition;
    JournalStatus status{JournalStatus::Prepared};
    std::vector<UndoRecord> records;
    std::string backup_identity;
    FailureReason failure_reason{FailureReason::None};
};

struct ExecutionResult {
    bool committed{};
    FailureReason failure_reason{FailureReason::None};
    std::size_t completed_moves{};
    std::size_t remaining_moves{};
    std::size_t completed_culls{};
    bool rollback_attempted{};
    bool rollback_succeeded{};
};

class ApprovedExecutionPlan final {
public:
    ApprovedExecutionPlan(const ApprovedExecutionPlan&) = default;
    ApprovedExecutionPlan& operator=(const ApprovedExecutionPlan&) = default;

    [[nodiscard]] const OperationId& Operation() const noexcept {
        return operation_id_;
    }
    [[nodiscard]] const OperationPrecondition& Precondition() const noexcept {
        return precondition_;
    }
    [[nodiscard]] const std::vector<ApprovedMove>& Moves() const noexcept {
        return moves_;
    }
    [[nodiscard]] const std::vector<ApprovedCull>& Culls() const noexcept {
        return culls_;
    }

private:
    friend class PreconditionValidator;

    ApprovedExecutionPlan(
        OperationId operation_id,
        OperationPrecondition precondition,
        std::vector<ApprovedMove> moves,
        std::vector<ApprovedCull> culls)
        : operation_id_(std::move(operation_id)),
          precondition_(std::move(precondition)),
          moves_(std::move(moves)),
          culls_(std::move(culls)) {}

    OperationId operation_id_;
    OperationPrecondition precondition_;
    std::vector<ApprovedMove> moves_;
    std::vector<ApprovedCull> culls_;
};

class ExecutionAuthorization final {
public:
    ExecutionAuthorization(const ExecutionAuthorization&) = default;
    ExecutionAuthorization& operator=(const ExecutionAuthorization&) = default;

    [[nodiscard]] const OperationId& Operation() const noexcept {
        return operation_id_;
    }
    [[nodiscard]] const CanonicalPlanDigest& PlanDigest() const noexcept {
        return plan_digest_;
    }
    [[nodiscard]] std::uint64_t Seal() const noexcept {
        return seal_;
    }

private:
    friend class PreconditionValidator;

    ExecutionAuthorization(
        OperationId operation_id,
        CanonicalPlanDigest plan_digest,
        std::uint64_t seal)
        : operation_id_(std::move(operation_id)),
          plan_digest_(std::move(plan_digest)),
          seal_(seal) {}

    OperationId operation_id_;
    CanonicalPlanDigest plan_digest_;
    std::uint64_t seal_{};
};

}  // namespace autocattery::execution
