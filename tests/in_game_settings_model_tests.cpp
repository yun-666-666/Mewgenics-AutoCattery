#include "in_game_settings_model.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "auto_cattery/settings_file_editor.hpp"
#include "test_support.hpp"

namespace autocattery::tests {
namespace {

std::string ReadRerollData(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    std::ostringstream output;
    output << stream.rdbuf();
    return output.str();
}

}  // namespace

void RunInGameSettingsModelTests() {
    const auto directory = std::filesystem::temp_directory_path() /
        "auto_cattery_stage18_in_game_settings_tests";
    std::filesystem::create_directories(directory);
    const auto user = directory / "user_config.json";
    const auto temporary = directory / "user_config.json.candidate.tmp";
    const auto data_mod = directory / "AutoCatteryData";
    std::filesystem::remove(user);
    std::filesystem::remove(temporary);

    std::filesystem::remove_all(data_mod);

    ui::InGameSettingsModel model({}, user, data_mod);
    AC_CHECK(static_cast<bool>(model.Reload()));
    AC_CHECK(model.PageCount() == 8);
    AC_CHECK(model.Rows(0).size() == 8);
    AC_CHECK(model.Rows(7).size() == 5);
    AC_CHECK(model.AllRows().size() == 48);
    AC_CHECK(model.DirectValue(0).has_value());
    AC_CHECK(!model.DirectValue(3).has_value());
    AC_CHECK(model.PageTitle(0).find("1/8") != std::string::npos);

    SettingsFileEditor reader({{}, user});
    const auto before = reader.Load();
    AC_CHECK(static_cast<bool>(before));
    AC_CHECK(static_cast<bool>(model.AdjustFlat(0, 1)));
    AC_CHECK(static_cast<bool>(model.Adjust(0, 3, 0)));
    AC_CHECK(static_cast<bool>(model.Adjust(6, 0, -1)));

    const auto after = reader.Load();
    AC_CHECK(static_cast<bool>(after));
    AC_CHECK(after.value.combat_scoring.recommended_count ==
             before.value.combat_scoring.recommended_count + 1);
    AC_CHECK(after.value.recommendation_marker.recommended_count ==
             after.value.combat_scoring.recommended_count);
    AC_CHECK(after.value.combat_scoring.exclude_kittens !=
             before.value.combat_scoring.exclude_kittens);
    AC_CHECK(after.value.room_planning.default_soft_capacity ==
             std::max<std::size_t>(
                 1, before.value.room_planning.default_soft_capacity - 1));
    AC_CHECK(!static_cast<bool>(model.Adjust(99, 0, 1)));
    AC_CHECK(static_cast<bool>(model.AdjustFlat(45, 1)));
    AC_CHECK(model.IsEnglish());
    AC_CHECK(static_cast<bool>(model.AdjustFlat(46, 1)));
    AC_CHECK(model.RequiresGameRestart(47));
    AC_CHECK(!model.RequiresGameRestart(46));
    AC_CHECK(static_cast<bool>(model.SetFlatValue(47, "9")));
    AC_CHECK(!static_cast<bool>(model.SetFlatValue(47, "100")));
    AC_CHECK(!static_cast<bool>(model.AdjustFlat(48, 1)));
    AC_CHECK(static_cast<bool>(model.SetFlatValue(0, "23")));
    AC_CHECK(static_cast<bool>(model.SetFlatValue(1, "12.50")));
    AC_CHECK(!static_cast<bool>(model.SetFlatValue(0, "wrong")));
    AC_CHECK(!static_cast<bool>(model.SetFlatValue(3, "1")));
    const auto direct = reader.Load();
    AC_CHECK(static_cast<bool>(direct));
    AC_CHECK(direct.value.combat_scoring.recommended_count == 23);
    AC_CHECK(direct.value.combat_scoring.minimum_score == 12.5);
    AC_CHECK(direct.value.recommendation_marker.recommended_count == 23);
    AC_CHECK(direct.value.general.language == "en-US");
    AC_CHECK(direct.value.diagnostics.collect_cat_data);
    AC_CHECK(direct.value.level_up.reroll_count == 9);
    const auto base_rerolls = ReadRerollData(
        data_mod / "data" / "classes" / "classes.gon.merge");
    const auto advanced_rerolls = ReadRerollData(
        data_mod / "data" / "classes" / "advanced_classes.gon.merge");
    AC_CHECK(std::count(
        base_rerolls.begin(), base_rerolls.end(), '\n') == 8);
    AC_CHECK(std::count(
        advanced_rerolls.begin(), advanced_rerolls.end(), '\n') == 8);
    AC_CHECK(base_rerolls.find("Fighter { innate_passives { AddLevelUpRerolls 9 } }") !=
             std::string::npos);
    AC_CHECK(advanced_rerolls.find("Jester { innate_passives { AddLevelUpRerolls 9 } }") !=
             std::string::npos);
    AC_CHECK(!std::filesystem::exists(temporary));
    std::filesystem::remove_all(data_mod);
}

}  // namespace autocattery::tests
