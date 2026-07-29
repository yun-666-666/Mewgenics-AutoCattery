#include "auto_cattery/execution/recovery_package.hpp"

#include <fstream>

#include <nlohmann/json.hpp>

#include "file_safety.hpp"

namespace autocattery::execution {

RecoveryPackageWriter::RecoveryPackageWriter(
    std::filesystem::path recovery_root)
    : recovery_root_(std::move(recovery_root)) {}

Result<std::filesystem::path> RecoveryPackageWriter::Write(
    const OperationId& operation_id,
    const BackupArtifact& backup) {
    if (!detail::IsSafeToken(operation_id) ||
        backup.backup_identity.empty()) {
        return {
            {},
            ErrorCode::WriteConflict,
            "recovery package identity is invalid"
        };
    }
    std::filesystem::path canonical_root;
    std::filesystem::path operation_root;
    if (!detail::PrepareContainedDirectory(
            recovery_root_, operation_id,
            canonical_root, operation_root)) {
        return {
            {},
            ErrorCode::WriteConflict,
            "recovery destination is not safely contained"
        };
    }
    const auto temporary = operation_root / L"recovery.json.tmp";
    const auto destination = operation_root / L"recovery.json";
    nlohmann::json document{
        {"schema_version", 1},
        {"operation_id", operation_id},
        {"backup_identity", backup.backup_identity},
        {"backup_hash", backup.content_hash},
        {"backup_size", backup.byte_size},
        {"automatic_live_restore_allowed", false},
        {"instructions",
         "Exit the game and use a separately verified recovery tool; "
         "never overwrite a running save."}
    };
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output << document.dump(2);
        output.flush();
        if (!output) {
            return {
                {},
                ErrorCode::WriteConflict,
                "recovery package write failed"
            };
        }
    }
    if (!detail::AtomicPublish(temporary, destination, true)) {
        return {
            {},
            ErrorCode::WriteConflict,
            "recovery package publication failed"
        };
    }
    return {destination};
}

}  // namespace autocattery::execution
