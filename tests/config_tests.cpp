#include "auto_cattery/config.hpp"

#include <filesystem>
#include <fstream>

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

std::filesystem::path TestDirectory() {
    const auto directory =
        std::filesystem::temp_directory_path() / "auto_cattery_phase01_tests";
    std::filesystem::create_directories(directory);
    return directory;
}

void Write(const std::filesystem::path& path, const char* contents) {
    std::ofstream stream(path, std::ios::trunc);
    stream << contents;
}

}  // namespace

void RunConfigTests() {
    const auto directory = TestDirectory();
    const auto defaults = directory / "default.json";
    const auto user = directory / "user.json";

    Write(defaults, R"({
        "schema_version": 1,
        "mod_enabled": true,
        "safe_mode": true,
        "log_level": "info",
        "language": "zh-CN",
        "ui": {},
        "safety": {
            "require_preview_before_destructive_actions": true,
            "create_backup_before_apply": true,
            "abort_on_unknown_game_build": true
        }
    })");
    Write(user, R"({"language":"en-US","unknown_future_field":42})");

    const auto valid = LoadConfig(defaults, user);
    AC_CHECK(static_cast<bool>(valid));
    AC_CHECK(valid.value.language == "en-US");
    AC_CHECK(valid.value.safe_mode);
    AC_CHECK(valid.value.ui.recommendation_button_enabled);
    AC_CHECK(valid.value.recommendation_marker.enabled);
    AC_CHECK(valid.value.recommendation_marker.show_rank);
    AC_CHECK(valid.value.recommendation_marker.show_score);
    AC_CHECK(valid.value.recommendation_marker.pulse);
    AC_CHECK(valid.value.recommendation_marker.pulse_period_ms == 900);
    AC_CHECK(valid.value.recommendation_marker.max_markers == 8);
    AC_CHECK(
        valid.value.recommendation_marker.fallback_to_text_prefix);

    Write(user, R"({"schema_version":99})");
    const auto future = LoadConfig(defaults, user);
    AC_CHECK(static_cast<bool>(future));
    AC_CHECK(future.value.force_read_only);

    Write(user, R"({"safety":{"create_backup_before_apply":false}})");
    const auto unsafe = LoadConfig(defaults, user);
    AC_CHECK(!static_cast<bool>(unsafe));
    AC_CHECK(unsafe.code == ErrorCode::ConfigInvalid);

    Write(user, "{");
    const auto truncated = LoadConfig(defaults, user);
    AC_CHECK(!static_cast<bool>(truncated));
}

}  // namespace autocattery::tests
