#include "auto_cattery/execution/backup_service.hpp"

#include <fstream>

#include "auto_cattery/save_safety/backup_manifest_store.hpp"
#include "auto_cattery/save_safety/file_hash.hpp"
#include "auto_cattery/save_safety/save_stability.hpp"
#include "auto_cattery/version.hpp"
#include "file_safety.hpp"

namespace autocattery::execution {
namespace {

Result<BackupArtifact> BackupError(std::string message) {
    return {{}, ErrorCode::BackupFailed, std::move(message)};
}

std::string SourceIdentity(const std::filesystem::path& path) {
    const auto wide = path.wstring();
    const std::string bytes(
        reinterpret_cast<const char*>(wide.data()),
        wide.size() * sizeof(wchar_t));
    const auto hash = save_safety::Sha256Text(bytes);
    return hash ? hash.value : std::string{};
}

Result<void> WriteVerification(
    const std::filesystem::path& path,
    const BackupArtifact& backup) {
    const auto temporary = path.wstring() + L".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            return {ErrorCode::BackupFailed,
                "backup verification file could not be opened"};
        }
        output << "sha256=" << backup.content_hash << '\n'
               << "byte_size=" << backup.byte_size << '\n'
               << "verification=passed\n";
        output.flush();
        if (!output) {
            return {ErrorCode::BackupFailed,
                "backup verification write failed"};
        }
    }
    if (!detail::AtomicPublish(temporary, path, false)) {
        return {ErrorCode::BackupFailed,
            "backup verification publication failed"};
    }
    return {};
}

}  // namespace

Result<std::string> HashFile(const std::filesystem::path& path) {
    return save_safety::Sha256File(path);
}

BackupService::BackupService(std::filesystem::path backup_root)
    : backup_root_(std::move(backup_root)) {}

Result<BackupArtifact> BackupService::CreateVerifiedBackup(
    const BackupRequest& request) {
    if (!detail::IsSafeToken(request.operation_id) ||
        !request.game_confirmed_quiescent) {
        return BackupError(
            "offline save state and safe operation identity are required");
    }
    const auto guard = save_safety::StableSaveGuard::Acquire(
        request.source_save, request.stable_window);
    if (!guard) {
        return BackupError(guard.message);
    }

    std::filesystem::path canonical_root;
    std::filesystem::path staging_root;
    if (!detail::PrepareContainedDirectory(
            backup_root_, request.operation_id + "-staging",
            canonical_root, staging_root)) {
        return BackupError("backup staging directory is not safely contained");
    }
    std::error_code error;
    const auto operation_root = canonical_root / request.operation_id;
    if (std::filesystem::exists(operation_root, error) || error ||
        guard.value.Path() == staging_root ||
        guard.value.Path().native().starts_with(staging_root.native())) {
        return BackupError("existing or unsafe backup destination");
    }

    const auto temporary = staging_root / L"original.savbak.tmp";
    const auto staged_backup = staging_root / L"original.savbak";
    std::filesystem::copy_file(
        guard.value.Path(), temporary,
        std::filesystem::copy_options::none, error);
    if (error) {
        return BackupError("backup copy failed");
    }
    const auto source_hash = HashFile(guard.value.Path());
    const auto backup_hash = HashFile(temporary);
    const auto source_size = guard.value.ByteSize();
    if (!source_hash || !backup_hash ||
        source_hash.value != backup_hash.value ||
        source_size != std::filesystem::file_size(temporary, error) || error) {
        return BackupError("backup verification failed");
    }
    if (!detail::AtomicPublish(temporary, staged_backup, false)) {
        return BackupError("atomic staged backup publication failed");
    }

    BackupArtifact artifact{
        .backup_file = operation_root / L"original.savbak",
        .content_hash = backup_hash.value,
        .byte_size = source_size,
        .backup_identity = request.operation_id + "-" +
            backup_hash.value.substr(0, 16),
        .manifest_file = operation_root / L"manifest.json",
        .source_identity = SourceIdentity(guard.value.Path())
    };
    if (artifact.source_identity.empty()) {
        return BackupError("source identity hashing failed");
    }
    const save_safety::BackupManifest manifest{
        .operation_id = request.operation_id,
        .created_utc = save_safety::UtcTimestamp(),
        .source_identity = artifact.source_identity,
        .game_build_identity = request.game_build_identity,
        .mod_version = std::string(kModVersion),
        .game_day = request.game_day,
        .game_confirmed_quiescent = true,
        .original_save = {
            .file_name = "original.savbak",
            .byte_size = artifact.byte_size,
            .sha256 = artifact.content_hash
        }
    };
    const auto manifest_written = save_safety::WriteBackupManifest(
        staging_root / L"manifest.json", manifest);
    if (!manifest_written) {
        return BackupError(manifest_written.message);
    }
    const auto verification_written = WriteVerification(
        staging_root / L"verification.txt", artifact);
    if (!verification_written) {
        return BackupError(verification_written.message);
    }
    if (!detail::AtomicPublish(staging_root, operation_root, false)) {
        return BackupError("atomic backup package publication failed");
    }
    const auto verified = VerifyBackup(artifact);
    if (!verified) {
        return BackupError(verified.message);
    }
    return {std::move(artifact)};
}

Result<void> BackupService::VerifyBackup(const BackupArtifact& backup) const {
    const auto manifest = save_safety::ReadBackupManifest(backup.manifest_file);
    if (!manifest ||
        manifest.value.source_identity != backup.source_identity ||
        manifest.value.original_save.sha256 != backup.content_hash ||
        manifest.value.original_save.byte_size != backup.byte_size ||
        backup.backup_file.filename() != L"original.savbak" ||
        backup.backup_file.parent_path() != backup.manifest_file.parent_path()) {
        return {ErrorCode::BackupFailed,
            "backup manifest does not match artifact"};
    }
    std::error_code error;
    if (!std::filesystem::is_regular_file(backup.backup_file, error) || error ||
        std::filesystem::file_size(backup.backup_file, error) !=
            backup.byte_size || error) {
        return {ErrorCode::BackupFailed, "backup file is unavailable"};
    }
    const auto hash = HashFile(backup.backup_file);
    if (!hash || hash.value != backup.content_hash) {
        return {ErrorCode::BackupFailed, "backup hash verification failed"};
    }
    return {};
}

}  // namespace autocattery::execution
