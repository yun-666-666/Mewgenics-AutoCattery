#pragma once

#include <chrono>
#include <thread>
#include <vector>

#include "auto_cattery/execution/transaction_executor.hpp"

namespace autocattery::tests::execution_test {

struct FakeWrite final : execution::IGameWriteAdapter {
    execution::WriteCapability capability{
        execution::WriteCapability::MoveAndCull};
    int calls{};
    int moves{};
    int culls{};
    int restores{};
    int fail_call{-1};
    int fail_restore{-1};
    std::chrono::milliseconds delay{};

    execution::WriteCapability Capability() const noexcept override {
        return capability;
    }

    Result<void> MoveCat(
        const execution::ApprovedMove&) override {
        return Call(false);
    }

    Result<void> CullCat(
        const execution::ApprovedCull&) override {
        return Call(true);
    }

    Result<void> Restore(
        const execution::UndoRecord&) override {
        ++restores;
        if (restores == fail_restore) {
            return {ErrorCode::WriteConflict, "restore failure"};
        }
        return {};
    }

private:
    Result<void> Call(bool cull) {
        if (delay.count() != 0) {
            std::this_thread::sleep_for(delay);
        }
        ++calls;
        if (calls == fail_call) {
            return {ErrorCode::WriteConflict, "injected failure"};
        }
        cull ? ++culls : ++moves;
        return {};
    }
};

struct FakeRead final : execution::IExecutionReadAdapter {
    execution::ExecutionObservation observation;
    int observe_calls{};
    int fail_observe{-1};
    int verify_move_calls{};
    int fail_move_verify{-1};
    int verify_cull_calls{};
    int fail_cull_verify{-1};
    int protected_checks{};
    int fail_protected_check{-1};

    Result<execution::ExecutionObservation> Observe() override {
        ++observe_calls;
        if (observe_calls == fail_observe) {
            observation.protection_digest++;
        }
        return {observation};
    }

    Result<void> VerifyMove(
        const execution::ApprovedMove&) override {
        ++verify_move_calls;
        if (verify_move_calls == fail_move_verify) {
            return {ErrorCode::WriteConflict, "move verify failure"};
        }
        return {};
    }

    Result<void> VerifyCull(
        const execution::ApprovedCull&,
        std::size_t expected) override {
        ++verify_cull_calls;
        if (verify_cull_calls == fail_cull_verify) {
            return {ErrorCode::WriteConflict, "cull verify failure"};
        }
        observation.cat_count = expected;
        return {};
    }

    Result<void> VerifyProtectedCatsPresent() override {
        ++protected_checks;
        if (protected_checks == fail_protected_check) {
            return {
                ErrorCode::WriteConflict,
                "protected population changed"
            };
        }
        return {};
    }
};

struct FakeBackup final : execution::IBackupService {
    bool succeed{true};
    int calls{};

    Result<execution::BackupArtifact> CreateVerifiedBackup(
        const execution::BackupRequest&) override {
        ++calls;
        if (!succeed) {
            return {{}, ErrorCode::BackupFailed, "backup failure"};
        }
        return {{
            .content_hash = "hash",
            .byte_size = 1,
            .backup_identity = "backup"
        }};
    }
};

struct FakeJournal final : execution::IJournalStore {
    int writes{};
    int fail_write{-1};
    std::vector<execution::JournalStatus> statuses;

    Result<void> Write(
        const execution::OperationJournalEntry& entry) override {
        ++writes;
        statuses.push_back(entry.status);
        if (writes == fail_write) {
            return {ErrorCode::WriteConflict, "journal failure"};
        }
        return {};
    }
};

struct FakeRecovery final : execution::IRecoveryPackageWriter {
    bool succeed{true};
    int calls{};

    Result<std::filesystem::path> Write(
        const execution::OperationId&,
        const execution::BackupArtifact&) override {
        ++calls;
        if (!succeed) {
            return {
                {},
                ErrorCode::WriteConflict,
                "recovery package failure"
            };
        }
        return {std::filesystem::path("recovery.json")};
    }
};

}  // namespace autocattery::tests::execution_test
