#include "auto_cattery/config.hpp"

#include <filesystem>
#include <fstream>

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

std::filesystem::path MigrationUserPath() {
    const auto directory =
        std::filesystem::temp_directory_path() /
        "auto_cattery_stage13_config_migration_tests";
    std::filesystem::create_directories(directory);
    return directory / "user.json";
}

Result<Config> LoadMigrationText(const char* contents) {
    const auto path = MigrationUserPath();
    std::ofstream stream(path, std::ios::trunc);
    stream << contents;
    stream.close();
    return LoadConfig({}, path);
}

}  // namespace

void RunConfigMigrationTests() {
    const auto legacy = LoadMigrationText(R"({
        "schema_version": 1,
        "mod_enabled": false,
        "safe_mode": true,
        "log_level": "warn",
        "language": "en-US",
        "safety": {
            "require_preview_before_destructive_actions": true,
            "create_backup_before_apply": false,
            "read_only_mode": true,
            "single_click_execute": false,
            "abort_on_unknown_game_build": true
        }
    })");
    AC_CHECK(static_cast<bool>(legacy));
    AC_CHECK(legacy.value.schema_version == 2);
    AC_CHECK(!legacy.value.general.mod_enabled);
    AC_CHECK(legacy.value.general.log_level == "warn");
    AC_CHECK(legacy.value.general.language == "en-US");
    AC_CHECK(!legacy.value.execution_safety.create_backup_before_apply);
    AC_CHECK(!legacy.value.execution.cull_enabled);

    const auto implicit_v1 = LoadMigrationText(R"({
        "language": "en-US",
        "combat_scoring": {"recommended_count": 3}
    })");
    AC_CHECK(static_cast<bool>(implicit_v1));
    AC_CHECK(implicit_v1.value.schema_version == 2);
    AC_CHECK(implicit_v1.value.language == "en-US");
    AC_CHECK(implicit_v1.value.combat_scoring.recommended_count == 3);

    const auto invalid_schema_type =
        LoadMigrationText(R"({"schema_version":"2"})");
    AC_CHECK(!static_cast<bool>(invalid_schema_type));
    AC_CHECK(invalid_schema_type.message.find("schema_version") !=
             std::string::npos);

    const auto invalid_old_schema =
        LoadMigrationText(R"({"schema_version":0})");
    AC_CHECK(!static_cast<bool>(invalid_old_schema));

    const auto unknown_module_version = LoadMigrationText(R"({
        "schema_version": 2,
        "combat_scoring": {"version": 2}
    })");
    AC_CHECK(!static_cast<bool>(unknown_module_version));
    AC_CHECK(unknown_module_version.message.find("combat_scoring.version") !=
             std::string::npos);

    const auto truncated = LoadMigrationText("{\"schema_version\":2,");
    AC_CHECK(!static_cast<bool>(truncated));
    AC_CHECK(truncated.message.find("user_config.json") != std::string::npos);
}

}  // namespace autocattery::tests
