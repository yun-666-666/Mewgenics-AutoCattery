#include "auto_cattery/save_safety/backup_catalog.hpp"

#include "auto_cattery/save_safety/backup_manifest_store.hpp"
#include "auto_cattery/save_safety/safe_path.hpp"

namespace autocattery::save_safety {

BackupCatalog::BackupCatalog(std::filesystem::path backup_root)
    : backup_root_(std::move(backup_root)) {}

Result<execution::BackupArtifact> BackupCatalog::Find(
    std::string_view operation_id) const {
    const auto directory = ResolveContainedExisting(backup_root_, operation_id);
    if (!directory) {
        return {{}, directory.code, directory.message};
    }
    const auto manifest_path = directory.value / L"manifest.json";
    const auto backup_file = directory.value / L"original.savbak";
    const auto manifest = ReadBackupManifest(manifest_path);
    if (!manifest || manifest.value.operation_id != operation_id) {
        return {{}, ErrorCode::BackupFailed, "backup manifest is missing or mismatched"};
    }
    std::error_code error;
    if (!std::filesystem::is_regular_file(backup_file, error) || error) {
        return {{}, ErrorCode::BackupFailed, "backup payload is unavailable"};
    }
    return {{
        .backup_file = backup_file,
        .content_hash = manifest.value.original_save.sha256,
        .byte_size = manifest.value.original_save.byte_size,
        .backup_identity = manifest.value.operation_id + "-" +
            manifest.value.original_save.sha256.substr(0, 16),
        .manifest_file = manifest_path,
        .source_identity = manifest.value.source_identity
    }};
}

std::vector<BackupListing> BackupCatalog::List() const {
    std::vector<BackupListing> listings;
    std::error_code error;
    if (!std::filesystem::is_directory(backup_root_, error) || error) {
        return listings;
    }
    for (std::filesystem::directory_iterator iterator(
             backup_root_, std::filesystem::directory_options::skip_permission_denied,
             error), end;
         iterator != end;
         iterator.increment(error)) {
        if (error) {
            error.clear();
            continue;
        }
        const auto name = iterator->path().filename().string();
        if (!iterator->is_directory(error) || error || !IsSafeOperationId(name)) {
            error.clear();
            continue;
        }
        const auto backup = Find(name);
        listings.push_back({
            .operation_id = name,
            .manifest_valid = static_cast<bool>(backup),
            .detail = backup ? "verified metadata" : backup.message
        });
    }
    return listings;
}

}  // namespace autocattery::save_safety
