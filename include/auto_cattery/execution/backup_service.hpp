#pragma once

#include <filesystem>
#include <string>

#include "auto_cattery/error.hpp"
#include "auto_cattery/execution/domain.hpp"

namespace autocattery::execution {

struct BackupArtifact {
    std::filesystem::path backup_file;
    std::string content_hash;
    std::uintmax_t byte_size{};
    std::string backup_identity;
};

struct BackupRequest {
    OperationId operation_id;
    std::filesystem::path source_save;
    bool game_confirmed_quiescent{};
};

class IBackupService {
public:
    virtual ~IBackupService() = default;
    virtual Result<BackupArtifact> CreateVerifiedBackup(
        const BackupRequest& request) = 0;
};

class BackupService final : public IBackupService {
public:
    explicit BackupService(std::filesystem::path backup_root);

    Result<BackupArtifact> CreateVerifiedBackup(
        const BackupRequest& request) override;

private:
    std::filesystem::path backup_root_;
};

[[nodiscard]] Result<std::string> HashFile(
    const std::filesystem::path& path);

}  // namespace autocattery::execution
