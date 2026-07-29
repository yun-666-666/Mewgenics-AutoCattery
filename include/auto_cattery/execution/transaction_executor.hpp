#pragma once

#include <filesystem>
#include <mutex>
#include <optional>
#include <unordered_set>

#include "auto_cattery/execution/backup_service.hpp"
#include "auto_cattery/execution/game_write_adapter.hpp"
#include "auto_cattery/execution/journal_store.hpp"
#include "auto_cattery/execution/precondition_validator.hpp"
#include "auto_cattery/execution/recovery_package.hpp"

namespace autocattery::execution {

struct ExecutionObservation {
    std::uint64_t scene_generation{};
    std::optional<std::int64_t> game_day;
    bool save_in_progress{};
    std::string game_build_identity;
    std::string save_identity;
    std::string snapshot_content_digest;
    std::string classification_digest;
    CanonicalPlanDigest plan_digest;
    std::uint64_t protection_digest{};
    std::size_t cat_count{};
};

class IExecutionReadAdapter {
public:
    virtual ~IExecutionReadAdapter() = default;
    virtual Result<ExecutionObservation> Observe() = 0;
    virtual Result<void> VerifyMove(const ApprovedMove& move) = 0;
    virtual Result<void> VerifyCull(
        const ApprovedCull& cull,
        std::size_t expected_cat_count) = 0;
    virtual Result<void> VerifyProtectedCatsPresent() = 0;
};

struct ExecutionRequest {
    const ApprovedExecutionPlan& plan;
    const ExecutionAuthorization& authorization;
    std::filesystem::path source_save;
    bool backup_source_quiescent{};
};

class TransactionExecutor final {
public:
    TransactionExecutor(
        IGameWriteAdapter& write_adapter,
        IExecutionReadAdapter& read_adapter,
        IBackupService& backup_service,
        IRecoveryPackageWriter& recovery_package_writer,
        IJournalStore& journal_store);

    [[nodiscard]] ExecutionResult Execute(
        const ExecutionRequest& request);

private:
    bool PreconditionsMatch(
        const OperationPrecondition& expected,
        const ExecutionObservation& actual,
        bool require_original_snapshot) const;
    bool Rollback(
        const ApprovedExecutionPlan& plan,
        OperationJournalEntry& journal,
        ExecutionResult& result);
    ExecutionResult StopAndRollback(
        const ApprovedExecutionPlan& plan,
        OperationJournalEntry& journal,
        ExecutionResult result,
        FailureReason reason);

    IGameWriteAdapter& write_adapter_;
    IExecutionReadAdapter& read_adapter_;
    IBackupService& backup_service_;
    IRecoveryPackageWriter& recovery_package_writer_;
    IJournalStore& journal_store_;
    std::mutex mutex_;
    std::unordered_set<OperationId> claimed_operations_;
};

}  // namespace autocattery::execution
