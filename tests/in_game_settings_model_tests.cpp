#include "in_game_settings_model.hpp"

#include <algorithm>
#include <filesystem>

#include "auto_cattery/settings_file_editor.hpp"
#include "test_support.hpp"

namespace autocattery::tests {

void RunInGameSettingsModelTests() {
    const auto directory = std::filesystem::temp_directory_path() /
        "auto_cattery_stage18_in_game_settings_tests";
    std::filesystem::create_directories(directory);
    const auto user = directory / "user_config.json";
    const auto temporary = directory / "user_config.json.candidate.tmp";
    std::filesystem::remove(user);
    std::filesystem::remove(temporary);

    ui::InGameSettingsModel model({}, user);
    AC_CHECK(static_cast<bool>(model.Reload()));
    AC_CHECK(model.PageCount() == 8);
    AC_CHECK(model.Rows(0).size() == 8);
    AC_CHECK(model.Rows(7).size() == 4);
    AC_CHECK(model.AllRows().size() == 47);
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
    AC_CHECK(!static_cast<bool>(model.AdjustFlat(47, 1)));
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
    AC_CHECK(!std::filesystem::exists(temporary));
}

}  // namespace autocattery::tests
