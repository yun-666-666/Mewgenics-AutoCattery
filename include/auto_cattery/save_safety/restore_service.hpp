#pragma once

#include <chrono>
#include <filesystem>

#include "auto_cattery/execution/backup_service.hpp"
#include "auto_cattery/save_safety/atomic_file_replace.hpp"
#include "auto_cattery/save_safety/backup_catalog.hpp"
#include "auto_cattery/save_safety/game_process_probe.hpp"

namespace autocattery::save_safety {

struct RestoreRequest {
    std::string backup_operation_id;
    std::string restore_operation_id;
    std::filesystem::path target_save;
    std::filesystem::path game_executable;
    std::chrono::milliseconds stable_window{150};
};

struct RestoreOutcome {
    execution::BackupArtifact restored_backup;
    execution::BackupArtifact pre_restore_backup;
    bool rollback_attempted{};
    bool rollback_succeeded{};
};

class RestoreService final {
public:
    RestoreService(
        BackupCatalog& catalog,
        execution::BackupService& backups,
        IGameProcessProbe& game_processes,
        IAtomicFileReplacer& replacer);

    [[nodiscard]] Result<execution::BackupArtifact> Verify(
        std::string_view backup_operation_id) const;
    [[nodiscard]] Result<RestoreOutcome> Restore(
        const RestoreRequest& request);

private:
    Result<void> RestoreFile(
        const execution::BackupArtifact& source,
        const std::filesystem::path& target,
        std::string_view temporary_tag,
        std::chrono::milliseconds stable_window) const;

    BackupCatalog& catalog_;
    execution::BackupService& backups_;
    IGameProcessProbe& game_processes_;
    IAtomicFileReplacer& replacer_;
};

}  // namespace autocattery::save_safety
