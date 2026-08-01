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
    AC_CHECK(model.Rows(7).size() == 2);
    AC_CHECK(model.PageTitle(0).find("1/8") != std::string::npos);

    SettingsFileEditor reader({{}, user});
    const auto before = reader.Load();
    AC_CHECK(static_cast<bool>(before));
    AC_CHECK(static_cast<bool>(model.Adjust(0, 0, 1)));
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
    AC_CHECK(!std::filesystem::exists(temporary));
}

}  // namespace autocattery::tests
