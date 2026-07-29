#include "auto_cattery/execution/transaction_executor.hpp"

#include <algorithm>

namespace autocattery::execution {

TransactionExecutor::TransactionExecutor(
    IGameWriteAdapter& write_adapter,
    IExecutionReadAdapter& read_adapter,
    IBackupService& backup_service,
    IRecoveryPackageWriter& recovery_package_writer,
    IJournalStore& journal_store)
    : write_adapter_(write_adapter),
      read_adapter_(read_adapter),
      backup_service_(backup_service),
      recovery_package_writer_(recovery_package_writer),
      journal_store_(journal_store) {}

bool TransactionExecutor::PreconditionsMatch(
    const OperationPrecondition& expected,
    const ExecutionObservation& actual,
    bool require_original_snapshot) const {
    return !actual.save_in_progress &&
           actual.scene_generation == expected.scene_generation &&
           actual.game_day == expected.game_day &&
           actual.game_build_identity ==
               expected.game_build_identity &&
           actual.save_identity == expected.save_identity &&
           actual.classification_digest ==
               expected.classification_digest &&
           actual.plan_digest == expected.plan_digest &&
           actual.protection_digest ==
               expected.protection_digest &&
           (!require_original_snapshot ||
            actual.snapshot_content_digest ==
                expected.snapshot_content_digest);
}

bool TransactionExecutor::Rollback(
    const ApprovedExecutionPlan&,
    OperationJournalEntry& journal,
    ExecutionResult& result) {
    result.rollback_attempted = !journal.records.empty();
    bool succeeded = true;
    for (auto record = journal.records.rbegin();
         record != journal.records.rend(); ++record) {
        if (!write_adapter_.Restore(*record)) {
            succeeded = false;
            break;
        }
    }
    result.rollback_succeeded = succeeded;
    journal.status = succeeded
        ? JournalStatus::RolledBack
        : JournalStatus::ManualRecoveryRequired;
    journal.failure_reason = succeeded
        ? result.failure_reason
        : FailureReason::RollbackFailed;
    if (!journal_store_.Write(journal)) {
        succeeded = false;
    }
    return succeeded;
}

ExecutionResult TransactionExecutor::StopAndRollback(
    const ApprovedExecutionPlan& plan,
    OperationJournalEntry& journal,
    ExecutionResult result,
    FailureReason reason) {
    result.failure_reason = reason;
    if (!Rollback(plan, journal, result)) {
        result.failure_reason = FailureReason::RollbackFailed;
    }
    return result;
}

ExecutionResult TransactionExecutor::Execute(
    const ExecutionRequest& request) {
    std::unique_lock lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock()) {
        return {.failure_reason = FailureReason::Busy};
    }
    if (!AuthorizationMatches(
            request.plan, request.authorization)) {
        return {
            .failure_reason = FailureReason::CancelAndRepreview
        };
    }
    if (!claimed_operations_.insert(
            request.plan.Operation()).second) {
        return {.failure_reason = FailureReason::AlreadyExecuted};
    }

    const auto capability = write_adapter_.Capability();
    if (capability == WriteCapability::Unsupported ||
        (!request.plan.Culls().empty() &&
         capability != WriteCapability::MoveAndCull)) {
        return {.failure_reason = FailureReason::Unsupported};
    }
    auto observation = read_adapter_.Observe();
    if (!observation ||
        !PreconditionsMatch(
            request.plan.Precondition(),
            observation.value, true)) {
        return {
            .failure_reason = FailureReason::PreconditionsChanged
        };
    }

    const auto backup = backup_service_.CreateVerifiedBackup({
        request.plan.Operation(),
        request.source_save,
        request.backup_source_quiescent
    });
    if (!backup) {
        return {.failure_reason = FailureReason::BackupFailed};
    }
    const auto recovery = recovery_package_writer_.Write(
        request.plan.Operation(), backup.value);
    if (!recovery) {
        return {
            .failure_reason = FailureReason::RecoveryPackageFailed
        };
    }

    OperationJournalEntry journal{
        .operation_id = request.plan.Operation(),
        .precondition = request.plan.Precondition(),
        .status = JournalStatus::Prepared,
        .backup_identity = backup.value.backup_identity
    };
    if (!journal_store_.Write(journal)) {
        return {.failure_reason = FailureReason::JournalFailed};
    }

    ExecutionResult result;
    for (std::size_t index = 0;
         index < request.plan.Moves().size(); ++index) {
        observation = read_adapter_.Observe();
        if (!observation ||
            !PreconditionsMatch(
                request.plan.Precondition(),
                observation.value, false)) {
            return StopAndRollback(
                request.plan, journal, result,
                FailureReason::PreconditionsChanged);
        }
        const auto& move = request.plan.Moves()[index];
        if (!write_adapter_.MoveCat(move)) {
            return StopAndRollback(
                request.plan, journal, result,
                FailureReason::MoveFailed);
        }
        journal.records.push_back({
            .operation_index = index,
            .old_room = move.from_room,
            .new_room = move.to_room
        });
        ++result.completed_moves;
        if (!read_adapter_.VerifyMove(move) ||
            !read_adapter_.VerifyProtectedCatsPresent()) {
            return StopAndRollback(
                request.plan, journal, result,
                FailureReason::MoveVerificationFailed);
        }
    }

    std::size_t expected_cat_count = observation.value.cat_count;
    for (std::size_t index = 0;
         index < request.plan.Culls().size(); ++index) {
        observation = read_adapter_.Observe();
        if (!observation ||
            !PreconditionsMatch(
                request.plan.Precondition(),
                observation.value, false) ||
            observation.value.cat_count != expected_cat_count) {
            return StopAndRollback(
                request.plan, journal, result,
                FailureReason::PreconditionsChanged);
        }
        const auto& cull = request.plan.Culls()[index];
        if (!write_adapter_.CullCat(cull)) {
            return StopAndRollback(
                request.plan, journal, result,
                FailureReason::CullFailed);
        }
        journal.records.push_back({
            .operation_index =
                request.plan.Moves().size() + index,
            .was_culled = true
        });
        ++result.completed_culls;
        --expected_cat_count;
        if (!read_adapter_.VerifyCull(cull, expected_cat_count) ||
            !read_adapter_.VerifyProtectedCatsPresent()) {
            return StopAndRollback(
                request.plan, journal, result,
                FailureReason::CullVerificationFailed);
        }
    }

    journal.status = JournalStatus::Committed;
    if (!journal_store_.Write(journal)) {
        return StopAndRollback(
            request.plan, journal, result,
            FailureReason::JournalFailed);
    }
    result.committed = true;
    return result;
}

}  // namespace autocattery::execution
