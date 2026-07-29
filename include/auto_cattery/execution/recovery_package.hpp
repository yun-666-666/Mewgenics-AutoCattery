#pragma once

#include <filesystem>

#include "auto_cattery/error.hpp"
#include "auto_cattery/execution/backup_service.hpp"
#include "auto_cattery/execution/domain.hpp"

namespace autocattery::execution {

class IRecoveryPackageWriter {
public:
    virtual ~IRecoveryPackageWriter() = default;
    virtual Result<std::filesystem::path> Write(
        const OperationId& operation_id,
        const BackupArtifact& backup) = 0;
};

class RecoveryPackageWriter final : public IRecoveryPackageWriter {
public:
    explicit RecoveryPackageWriter(std::filesystem::path recovery_root);

    Result<std::filesystem::path> Write(
        const OperationId& operation_id,
        const BackupArtifact& backup) override;

private:
    std::filesystem::path recovery_root_;
};

}  // namespace autocattery::execution
