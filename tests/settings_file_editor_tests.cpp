#include "auto_cattery/settings_file_editor.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

std::string ReadText(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    std::ostringstream text;
    text << stream.rdbuf();
    return text.str();
}

}  // namespace

void RunSettingsFileEditorTests() {
    const auto directory = std::filesystem::temp_directory_path() /
        "auto_cattery_stage13_settings_file_editor_tests";
    std::filesystem::create_directories(directory);
    const auto user = directory / "user_config.json";
    const auto temporary = directory / "user_config.json.candidate.tmp";
    std::filesystem::remove(user);
    std::filesystem::remove(temporary);
    {
        std::ofstream stream(user, std::ios::binary | std::ios::trunc);
        stream << R"({"custom_note":"preserve"})";
    }

    SettingsFileEditor editor({{}, user});
    const auto initial = editor.Load();
    AC_CHECK(static_cast<bool>(initial));

    auto changed = initial.value;
    changed.combat_scoring.recommended_count = 11;
    changed.recommendation_marker.recommended_count = 11;
    changed.combat_scoring.stat_weights[0] = 2.75;
    changed.breeding_scoring.stat_weights[6] = -1.5;
    changed.classification.minimum_general_reserve = 6;
    changed.room_planning.default_soft_capacity = 5;
    changed.room_planning.allow_soft_overflow = false;
    changed.recommendation_marker.show_score = false;
    changed.general.language = "en-US";
    changed.language = "en-US";
    changed.diagnostics.collect_cat_data = true;
    const auto saved = editor.Save(changed);
    AC_CHECK(static_cast<bool>(saved));
    AC_CHECK(saved.value.combat_scoring.recommended_count == 11);
    AC_CHECK(saved.value.combat_scoring.stat_weights[0] == 2.75);
    AC_CHECK(saved.value.breeding_scoring.stat_weights[6] == -1.5);
    AC_CHECK(saved.value.classification.minimum_general_reserve == 6);
    AC_CHECK(saved.value.room_planning.default_soft_capacity == 5);
    AC_CHECK(!saved.value.room_planning.allow_soft_overflow);
    AC_CHECK(!saved.value.recommendation_marker.show_score);
    AC_CHECK(saved.value.general.language == "en-US");
    AC_CHECK(saved.value.diagnostics.collect_cat_data);
    AC_CHECK(!std::filesystem::exists(temporary));

    const auto reloaded = editor.Load();
    AC_CHECK(static_cast<bool>(reloaded));
    AC_CHECK(reloaded.value.combat_scoring.recommended_count == 11);
    AC_CHECK(reloaded.value.combat_scoring.stat_weights[0] == 2.75);
    AC_CHECK(reloaded.value.breeding_scoring.stat_weights[6] == -1.5);
    AC_CHECK(reloaded.value.classification.minimum_general_reserve == 6);
    AC_CHECK(reloaded.value.room_planning.default_soft_capacity == 5);
    AC_CHECK(!reloaded.value.room_planning.allow_soft_overflow);
    AC_CHECK(!reloaded.value.recommendation_marker.show_score);
    AC_CHECK(reloaded.value.general.language == "en-US");
    AC_CHECK(reloaded.value.diagnostics.collect_cat_data);

    const auto stored = nlohmann::json::parse(ReadText(user));
    AC_CHECK(stored.at("custom_note") == "preserve");
    AC_CHECK(stored.at("combat_scoring").at("recommended_count") == 11);
    AC_CHECK(stored.at("combat_scoring").at("stat_weights")
        .at("strength") == 2.75);

    const auto before_invalid = ReadText(user);
    changed.combat_scoring.recommended_count = 0;
    changed.recommendation_marker.recommended_count = 0;
    const auto invalid = editor.Save(changed);
    AC_CHECK(!static_cast<bool>(invalid));
    AC_CHECK(invalid.code == ErrorCode::ConfigInvalid);
    AC_CHECK(ReadText(user) == before_invalid);
    AC_CHECK(!std::filesystem::exists(temporary));
}

}  // namespace autocattery::tests
