#include "auto_cattery/save_safety/backup_manifest_store.hpp"

#include <windows.h>

#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>

#include <nlohmann/json.hpp>

#include "../execution/file_safety.hpp"

namespace autocattery::save_safety {
namespace {

bool SafeToken(std::string_view value) {
    return !value.empty() && std::ranges::all_of(
        value, [](unsigned char byte) {
            return std::isalnum(byte) || byte == '-' || byte == '_';
        });
}

bool IsSha256(std::string_view value) {
    return value.size() == 64 && std::ranges::all_of(
        value, [](unsigned char byte) { return std::isxdigit(byte) != 0; });
}

Result<BackupManifest> Invalid(std::string message) {
    return {{}, ErrorCode::BackupFailed, std::move(message)};
}

}  // namespace

std::string UtcTimestamp() {
    SYSTEMTIME now{};
    GetSystemTime(&now);
    std::ostringstream output;
    output << std::setfill('0')
           << std::setw(4) << now.wYear << '-'
           << std::setw(2) << now.wMonth << '-'
           << std::setw(2) << now.wDay << 'T'
           << std::setw(2) << now.wHour << ':'
           << std::setw(2) << now.wMinute << ':'
           << std::setw(2) << now.wSecond << '.'
           << std::setw(3) << now.wMilliseconds << 'Z';
    return output.str();
}

Result<void> WriteBackupManifest(
    const std::filesystem::path& path,
    const BackupManifest& manifest) {
    if (manifest.schema_version != 1 ||
        !SafeToken(manifest.operation_id) ||
        !IsSha256(manifest.source_identity) ||
        manifest.original_save.file_name != "original.savbak" ||
        !IsSha256(manifest.original_save.sha256) ||
        manifest.created_utc.empty() || manifest.mod_version.empty()) {
        return {ErrorCode::BackupFailed, "backup manifest is invalid"};
    }
    const nlohmann::json document{
        {"schema_version", manifest.schema_version},
        {"operation_id", manifest.operation_id},
        {"created_utc", manifest.created_utc},
        {"source_identity", manifest.source_identity},
        {"game_build_identity", manifest.game_build_identity},
        {"mod_version", manifest.mod_version},
        {"game_day", manifest.game_day},
        {"game_confirmed_quiescent", manifest.game_confirmed_quiescent},
        {"original_save", {
            {"file_name", manifest.original_save.file_name},
            {"byte_size", manifest.original_save.byte_size},
            {"sha256", manifest.original_save.sha256}
        }}
    };
    const auto temporary = path.wstring() + L".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            return {ErrorCode::BackupFailed, "backup manifest temporary file could not be opened"};
        }
        output << document.dump(2) << '\n';
        output.flush();
        if (!output) {
            return {ErrorCode::BackupFailed, "backup manifest write failed"};
        }
    }
    if (!execution::detail::AtomicPublish(temporary, path, false)) {
        return {ErrorCode::BackupFailed, "backup manifest publication failed"};
    }
    return {};
}

Result<BackupManifest> ReadBackupManifest(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return Invalid("backup manifest is unavailable");
    }
    try {
        const auto document = nlohmann::json::parse(input);
        if (!document.is_object() ||
            document.value("schema_version", 0) != 1 ||
            !document.contains("original_save") ||
            !document.at("original_save").is_object()) {
            return Invalid("backup manifest schema is unsupported");
        }
        BackupManifest manifest;
        manifest.operation_id = document.at("operation_id").get<std::string>();
        manifest.created_utc = document.at("created_utc").get<std::string>();
        manifest.source_identity = document.at("source_identity").get<std::string>();
        manifest.game_build_identity = document.at("game_build_identity").get<std::string>();
        manifest.mod_version = document.at("mod_version").get<std::string>();
        if (!document.at("game_day").is_null()) {
            manifest.game_day = document.at("game_day").get<std::int64_t>();
        }
        manifest.game_confirmed_quiescent = document.at("game_confirmed_quiescent").get<bool>();
        const auto& original = document.at("original_save");
        manifest.original_save.file_name = original.at("file_name").get<std::string>();
        manifest.original_save.byte_size = original.at("byte_size").get<std::uintmax_t>();
        manifest.original_save.sha256 = original.at("sha256").get<std::string>();
        if (!SafeToken(manifest.operation_id) ||
            !IsSha256(manifest.source_identity) ||
            manifest.original_save.file_name != "original.savbak" ||
            !IsSha256(manifest.original_save.sha256) ||
            manifest.created_utc.empty() || manifest.mod_version.empty()) {
            return Invalid("backup manifest contains unsafe data");
        }
        return {std::move(manifest)};
    } catch (const nlohmann::json::exception&) {
        return Invalid("backup manifest is malformed");
    }
}

}  // namespace autocattery::save_safety
