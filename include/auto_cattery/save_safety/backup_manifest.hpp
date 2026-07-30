#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace autocattery::save_safety {

struct BackupFileManifest {
    std::string file_name;
    std::uintmax_t byte_size{};
    std::string sha256;
};

struct BackupManifest {
    int schema_version{1};
    std::string operation_id;
    std::string created_utc;
    std::string source_identity;
    std::string game_build_identity;
    std::string mod_version;
    std::optional<std::int64_t> game_day;
    bool game_confirmed_quiescent{};
    BackupFileManifest original_save;
};

}  // namespace autocattery::save_safety
