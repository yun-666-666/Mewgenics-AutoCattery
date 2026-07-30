#include "auto_cattery/save_safety/restore_service.hpp"

#include "auto_cattery/save_safety/file_hash.hpp"
#include "auto_cattery/save_safety/safe_path.hpp"
#include "auto_cattery/save_safety/save_stability.hpp"

namespace autocattery::save_safety {
namespace {

Result<void> MatchFile(
    const std::filesystem::path& path,
    const execution::BackupArtifact& expected) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error) || error ||
        std::filesystem::file_size(path, error) != expected.byte_size || error) {
        return {ErrorCode::WriteConflict, "save file size does not match backup"};
    }
    const auto hash = Sha256File(path);
    if (!hash || hash.value != expected.content_hash) {
        return {ErrorCode::WriteConflict, "save file hash does not match backup"};
    }
    return {};
}

}  // namespace

RestoreService::RestoreService(
    BackupCatalog& catalog,
    execution::BackupService& backups,
    IGameProcessProbe& game_processes,
    IAtomicFileReplacer& replacer)
    : catalog_(catalog),
      backups_(backups),
      game_processes_(game_processes),
      replacer_(replacer) {}

Result<execution::BackupArtifact> RestoreService::Verify(
    std::string_view backup_operation_id) const {
    const auto backup = catalog_.Find(backup_operation_id);
    if (!backup) {
        return backup;
    }
    const auto verified = backups_.VerifyBackup(backup.value);
    if (!verified) {
        return {{}, verified.code, verified.message};
    }
    return backup;
}

Result<void> RestoreService::RestoreFile(
    const execution::BackupArtifact& source,
    const std::filesystem::path& target,
    std::string_view temporary_tag,
    std::chrono::milliseconds stable_window) const {
    if (!IsSafeOperationId(temporary_tag)) {
        return {ErrorCode::WriteConflict, "restore temporary identity is unsafe"};
    }
    const auto temporary = target.parent_path() /
        (target.filename().wstring() + L".auto-cattery-" +
            std::wstring(temporary_tag.begin(), temporary_tag.end()) + L".tmp");
    std::error_code error;
    {
        const auto guard = StableSaveGuard::Acquire(target, stable_window);
        if (!guard) {
            return {guard.code, guard.message};
        }
        if (std::filesystem::exists(temporary, error) || error) {
            return {ErrorCode::WriteConflict,
                "restore temporary file already exists and was retained"};
        }
        std::filesystem::copy_file(
            source.backup_file, temporary,
            std::filesystem::copy_options::none, error);
        if (error) {
            return {ErrorCode::WriteConflict, "restore temporary copy failed"};
        }
        const auto staged = MatchFile(temporary, source);
        if (!staged) {
            return staged;
        }
    }
    const auto replaced = replacer_.ReplaceTemporary(temporary, target);
    if (!replaced) {
        return replaced;
    }
    return MatchFile(target, source);
}

Result<RestoreOutcome> RestoreService::Restore(const RestoreRequest& request) {
    if (!IsSafeOperationId(request.backup_operation_id) ||
        !IsSafeOperationId(request.restore_operation_id) ||
        request.target_save.extension() != L".sav") {
        return {{}, ErrorCode::WriteConflict, "restore request is unsafe"};
    }
    const auto game_running = game_processes_.IsRunning(request.game_executable);
    if (!game_running) {
        return {{}, game_running.code, game_running.message};
    }
    if (game_running.value) {
        return {{}, ErrorCode::OperationCancelled,
            "restore is blocked while the verified game executable is running"};
    }
    const auto backup = Verify(request.backup_operation_id);
    if (!backup) {
        return {{}, backup.code, backup.message};
    }
    const auto pre_restore = backups_.CreateVerifiedBackup({
        .operation_id = request.restore_operation_id,
        .source_save = request.target_save,
        .game_confirmed_quiescent = true,
        .game_build_identity = "unknown",
        .stable_window = request.stable_window
    });
    if (!pre_restore) {
        return {{}, pre_restore.code,
            "pre-restore backup failed: " + pre_restore.message};
    }
    const auto still_closed = game_processes_.IsRunning(request.game_executable);
    if (!still_closed) {
        return {{}, still_closed.code, still_closed.message};
    }
    if (still_closed.value) {
        return {{}, ErrorCode::OperationCancelled,
            "restore is blocked because the game started during preparation"};
    }
    const auto restored = RestoreFile(
        backup.value, request.target_save, request.restore_operation_id,
        request.stable_window);
    if (restored) {
        return {{
            .restored_backup = backup.value,
            .pre_restore_backup = pre_restore.value
        }};
    }

    RestoreOutcome outcome{
        .restored_backup = backup.value,
        .pre_restore_backup = pre_restore.value,
        .rollback_attempted = true
    };
    const auto rollback = RestoreFile(
        pre_restore.value, request.target_save,
        request.restore_operation_id + "-rollback", request.stable_window);
    outcome.rollback_succeeded = static_cast<bool>(rollback);
    return {
        {},
        ErrorCode::WriteConflict,
        outcome.rollback_succeeded
            ? "restore readback failed; pre-restore backup was restored"
            : "restore failed and automatic rollback failed; manual recovery is required"
    };
}

}  // namespace autocattery::save_safety
