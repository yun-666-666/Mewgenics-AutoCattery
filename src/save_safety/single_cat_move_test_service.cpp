#include "auto_cattery/save_safety/single_cat_move_test_service.hpp"

#include "auto_cattery/save_safety/backup_catalog.hpp"
#include "auto_cattery/save_safety/file_hash.hpp"
#include "auto_cattery/save_safety/restore_service.hpp"
#include "auto_cattery/save_safety/safe_path.hpp"
#include "auto_cattery/save_safety/test_copy_guard.hpp"
#include "auto_cattery/snapshot/house_state_writer.hpp"

namespace autocattery::save_safety {
namespace {

Result<SingleCatMoveTestOutcome> MoveFailure(std::string message) {
    return {{}, ErrorCode::WriteConflict, std::move(message)};
}

}  // namespace

SingleCatMoveTestService::SingleCatMoveTestService(
    const IGameBuildGate& build_gate,
    IGameProcessProbe& processes,
    IAtomicFileReplacer& replacer,
    const TestCopyHouseStateStore& store)
    : build_gate_(build_gate),
      processes_(processes),
      replacer_(replacer),
      store_(store) {}

Result<void> SingleCatMoveTestService::RollBackReadbackFailure(
    const SingleCatMoveTestRequest& request,
    const std::filesystem::path& target,
    execution::BackupService& backups) const {
    BackupCatalog catalog(request.test_root / L"backups");
    RestoreService restore(catalog, backups, processes_, replacer_);
    const auto restored = restore.Restore({
        .backup_operation_id = request.operation_id,
        .restore_operation_id = request.operation_id + "-auto-rollback",
        .target_save = target,
        .game_executable = request.game_executable,
        .stable_window = request.stable_window
    });
    return restored
        ? Result<void>{}
        : Result<void>{restored.code, restored.message};
}

Result<SingleCatMoveTestOutcome> SingleCatMoveTestService::MoveOne(
    const SingleCatMoveTestRequest& request) {
    if (!request.development_test_enabled ||
        !IsSafeOperationId(request.operation_id)) {
        return MoveFailure(
            "explicit development test enablement and a safe operation ID are required");
    }
    const auto build = build_gate_.Verify(request.game_executable);
    if (!build) {
        return {{}, build.code, build.message};
    }
    const auto target = ResolveIsolatedTestSave(
        request.test_root, request.test_save, request.game_executable);
    if (!target) {
        return {{}, target.code, target.message};
    }
    const auto running = processes_.IsRunning(request.game_executable);
    if (!running || running.value) {
        return {{}, ErrorCode::OperationCancelled,
            running ? "single-cat test is blocked while the game is running"
                    : running.message};
    }
    const auto original_blob = store_.Read(target.value);
    if (!original_blob) {
        return {{}, original_blob.code, original_blob.message};
    }
    const auto relocation = snapshot::BuildSingleCatTestRelocation(
        original_blob.value, request.moved_index,
        request.placement_source_index);
    if (!relocation) {
        return {{}, relocation.code, relocation.message};
    }

    execution::BackupService backups(request.test_root / L"backups");
    const auto backup = backups.CreateVerifiedBackup({
        .operation_id = request.operation_id,
        .source_save = target.value,
        .game_confirmed_quiescent = true,
        .game_build_identity = build.value,
        .stable_window = request.stable_window
    });
    if (!backup) {
        return {{}, backup.code, backup.message};
    }
    const auto temporary = target.value.parent_path() /
        (target.value.filename().wstring() + L".auto-cattery-" +
            std::wstring(request.operation_id.begin(), request.operation_id.end()) +
            L".tmp");
    std::error_code error;
    if (std::filesystem::exists(temporary, error) || error) {
        return MoveFailure(
            "single-cat test temporary file already exists and was retained");
    }
    std::filesystem::copy_file(
        target.value, temporary, std::filesystem::copy_options::none, error);
    if (error) {
        return MoveFailure("single-cat test temporary copy failed");
    }
    const auto written = store_.Write(
        temporary, relocation.value.encoded_house_state);
    if (!written) {
        return MoveFailure(written.message);
    }
    const auto staged_readback = store_.Read(temporary);
    if (!staged_readback ||
        staged_readback.value != relocation.value.encoded_house_state) {
        return MoveFailure(
            "single-cat test staged independent readback did not match");
    }

    const auto still_closed = processes_.IsRunning(request.game_executable);
    if (!still_closed || still_closed.value) {
        return {{}, ErrorCode::OperationCancelled,
            still_closed
                ? "single-cat test is blocked because the game started during preparation"
                : still_closed.message};
    }
    const auto unchanged_hash = Sha256File(target.value);
    if (!unchanged_hash || unchanged_hash.value != backup.value.content_hash) {
        return MoveFailure("test save changed after backup; publication was cancelled");
    }
    const auto replaced = replacer_.ReplaceTemporary(temporary, target.value);
    if (!replaced) {
        return MoveFailure(replaced.message);
    }
    const auto final_readback = store_.Read(target.value);
    if (!final_readback ||
        final_readback.value != relocation.value.encoded_house_state) {
        const auto rollback = RollBackReadbackFailure(
            request, target.value, backups);
        return MoveFailure(rollback
            ? "single-cat readback failed; the verified backup was restored"
            : "single-cat readback and automatic rollback failed; manual recovery is required");
    }
    return {{
        .backup = backup.value,
        .build_identity = build.value,
        .original_room = relocation.value.original.room_id,
        .target_room = relocation.value.relocated.room_id,
        .independent_readback_verified = true
    }};
}

}  // namespace autocattery::save_safety
