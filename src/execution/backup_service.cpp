#include "auto_cattery/execution/backup_service.hpp"

#include <windows.h>
#include <bcrypt.h>

#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

#include "file_safety.hpp"

namespace autocattery::execution {
namespace {

Result<BackupArtifact> BackupError(std::string message) {
    return {
        {},
        ErrorCode::BackupFailed,
        std::move(message)
    };
}

}  // namespace

Result<std::string> HashFile(const std::filesystem::path& path) {
    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash{};
    std::vector<unsigned char> hash_object;
    std::array<unsigned char, 32> digest{};
    std::ifstream input(path, std::ios::binary);
    if (!input ||
        BCryptOpenAlgorithmProvider(
            &algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) {
        return {{}, ErrorCode::BackupFailed, "hash initialization failed"};
    }

    DWORD object_size{};
    DWORD result_size{};
    if (BCryptGetProperty(
            algorithm, BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&object_size),
            sizeof(object_size), &result_size, 0) < 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return {{}, ErrorCode::BackupFailed, "hash metadata failed"};
    }
    hash_object.resize(object_size);
    if (BCryptCreateHash(
            algorithm, &hash, hash_object.data(), object_size,
            nullptr, 0, 0) < 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return {{}, ErrorCode::BackupFailed, "hash creation failed"};
    }

    std::array<char, 64 * 1024> buffer{};
    while (input) {
        input.read(buffer.data(), buffer.size());
        const auto count = input.gcount();
        if (count > 0 &&
            BCryptHashData(
                hash,
                reinterpret_cast<PUCHAR>(buffer.data()),
                static_cast<ULONG>(count), 0) < 0) {
            BCryptDestroyHash(hash);
            BCryptCloseAlgorithmProvider(algorithm, 0);
            return {{}, ErrorCode::BackupFailed, "hash update failed"};
        }
    }
    if (!input.eof() ||
        BCryptFinishHash(
            hash, digest.data(),
            static_cast<ULONG>(digest.size()), 0) < 0) {
        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return {{}, ErrorCode::BackupFailed, "hash finalization failed"};
    }
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);

    std::ostringstream encoded;
    encoded << std::hex << std::setfill('0');
    for (const auto byte : digest) {
        encoded << std::setw(2) << static_cast<unsigned int>(byte);
    }
    return {encoded.str()};
}

BackupService::BackupService(std::filesystem::path backup_root)
    : backup_root_(std::move(backup_root)) {}

Result<BackupArtifact> BackupService::CreateVerifiedBackup(
    const BackupRequest& request) {
    if (!detail::IsSafeToken(request.operation_id) ||
        !request.game_confirmed_quiescent ||
        request.source_save.extension() != L".sav") {
        return BackupError(
            "offline save state and safe operation identity are required");
    }

    std::error_code error;
    const auto source =
        std::filesystem::weakly_canonical(request.source_save, error);
    if (error || !std::filesystem::is_regular_file(source, error)) {
        return BackupError("source save is unavailable");
    }
    auto wal = source;
    wal += L"-wal";
    auto shm = source;
    shm += L"-shm";
    if (std::filesystem::exists(wal, error) ||
        std::filesystem::exists(shm, error)) {
        return BackupError(
            "SQLite WAL/SHM sidecars make single-file backup unsupported");
    }

    std::filesystem::path canonical_root;
    std::filesystem::path operation_root;
    if (!detail::PrepareContainedDirectory(
            backup_root_, request.operation_id,
            canonical_root, operation_root) ||
        source == operation_root ||
        source.native().starts_with(operation_root.native())) {
        return BackupError("backup destination is not safely contained");
    }

    const auto temporary = operation_root / L"original.savbak.tmp";
    const auto destination = operation_root / L"original.savbak";
    if (std::filesystem::exists(destination, error)) {
        return BackupError("existing backup will not be overwritten");
    }

    std::filesystem::copy_file(
        source, temporary,
        std::filesystem::copy_options::none, error);
    if (error) {
        return BackupError("backup copy failed");
    }
    const auto source_hash = HashFile(source);
    const auto backup_hash = HashFile(temporary);
    const auto source_size = std::filesystem::file_size(source, error);
    if (!source_hash || !backup_hash || error ||
        source_hash.value != backup_hash.value ||
        source_size != std::filesystem::file_size(temporary, error) ||
        error) {
        return BackupError("backup verification failed");
    }
    if (!detail::AtomicPublish(temporary, destination, false)) {
        return BackupError("atomic backup publication failed");
    }
    return {{
        .backup_file = destination,
        .content_hash = backup_hash.value,
        .byte_size = source_size,
        .backup_identity =
            request.operation_id + "-" +
            backup_hash.value.substr(0, 16)
    }};
}

}  // namespace autocattery::execution
