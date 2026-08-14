#include "in_game_settings_model.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

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

std::vector<std::string> ReadLines(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    std::vector<std::string> lines;
    for (std::string line; std::getline(stream, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) lines.push_back(std::move(line));
    }
    return lines;
}

}  // namespace

void RunInGameSettingsModelTests() {
    const auto directory = std::filesystem::temp_directory_path() /
        "auto_cattery_stage18_in_game_settings_tests";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto user = directory / "user_config.json";
    const auto temporary = directory / "user_config.json.candidate.tmp";
    const auto data_mod = directory / "AutoCattery";
    const auto compatible_data_mod = directory / "SkillsPassivesFirstData";
    std::filesystem::create_directories(
        compatible_data_mod / "data" / "classes");
    {
        std::ofstream base(
            compatible_data_mod / "data" / "classes" /
                "classes.gon.merge",
            std::ios::binary);
        base << "Fighter { innate_passives { AddLevelUpRerolls 3 } }\n";
        std::ofstream advanced(
            compatible_data_mod / "data" / "classes" /
                "advanced_classes.gon.merge",
            std::ios::binary);
        advanced << "Monk { innate_passives { AddLevelUpRerolls 3 } }\n";
    }
    {
        std::ofstream mod_list(directory / "modlist.txt", std::ios::binary);
        mod_list << "AutoCattery\nSkillsPassivesFirstData\nAutoCattery\n";
    }

    ui::InGameSettingsModel model({}, user, data_mod);
    AC_CHECK(static_cast<bool>(model.Reload()));
    AC_CHECK(model.PageCount() == 8);
    AC_CHECK(model.Rows(0).size() == 8);
    AC_CHECK(model.Rows(7).size() == 5);
    AC_CHECK(model.AllRows().size() == 48);
    AC_CHECK(model.FurnitureRows().size() == 48);
    AC_CHECK(model.FurnitureRows()[4].empty());
    AC_CHECK(model.FurnitureRows()[17].find("-8.00") != std::string::npos);
    for (const auto& row : model.FurnitureRows()) {
        AC_CHECK(row.find("/猫") == std::string::npos);
        AC_CHECK(row.find("每猫") == std::string::npos);
    }
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
    for (const auto& row : model.FurnitureRows()) {
        AC_CHECK(row.find("/cat") == std::string::npos);
        AC_CHECK(row.find("per cat") == std::string::npos);
    }
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
    AC_CHECK(model.DirectFurnitureValue(17).has_value());
    AC_CHECK(!model.DirectFurnitureValue(41).has_value());
    AC_CHECK(static_cast<bool>(model.SetFurnitureValue(17, "-3.50")));
    AC_CHECK(static_cast<bool>(model.SetFurnitureValue(20, "12.00")));
    AC_CHECK(static_cast<bool>(model.SetFurnitureValue(40, "25")));
    AC_CHECK(static_cast<bool>(model.AdjustFurniture(41, 0)));
    AC_CHECK(!static_cast<bool>(model.SetFurnitureValue(40, "101")));
    AC_CHECK(!static_cast<bool>(model.AdjustFurniture(4, 1)));
    const auto direct = reader.Load();
    AC_CHECK(static_cast<bool>(direct));
    AC_CHECK(direct.value.combat_scoring.recommended_count == 23);
    AC_CHECK(direct.value.combat_scoring.minimum_score == 12.5);
    AC_CHECK(direct.value.recommendation_marker.recommended_count == 23);
    AC_CHECK(direct.value.general.language == "en-US");
    AC_CHECK(direct.value.diagnostics.collect_cat_data);
    AC_CHECK(direct.value.level_up.reroll_count == 9);
    AC_CHECK(direct.value.furniture_placement.combat.comfort_per_resident == -3.5);
    AC_CHECK(direct.value.furniture_placement.combat.mutation_per_resident == 12.0);
    AC_CHECK(direct.value.furniture_placement.minimum_furnishing_coverage_percent == 25);
    AC_CHECK(direct.value.furniture_placement.fill_remaining_capacity);
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
    const auto compatible_base_rerolls = ReadRerollData(
        compatible_data_mod / "data" / "classes" / "classes.gon.merge");
    const auto compatible_advanced_rerolls = ReadRerollData(
        compatible_data_mod / "data" / "classes" /
            "advanced_classes.gon.merge");
    AC_CHECK(compatible_base_rerolls.find(
        "Fighter { innate_passives { AddLevelUpRerolls 9 } }") !=
        std::string::npos);
    AC_CHECK(compatible_advanced_rerolls.find(
        "Jester { innate_passives { AddLevelUpRerolls 9 } }") !=
        std::string::npos);
    const auto enabled_mods = ReadLines(directory / "modlist.txt");
    AC_CHECK(enabled_mods.size() == 2);
    AC_CHECK(enabled_mods[0] == "SkillsPassivesFirstData");
    AC_CHECK(enabled_mods[1] == "AutoCattery");
    AC_CHECK(!std::filesystem::exists(temporary));
    std::filesystem::remove_all(directory);
}

}  // namespace autocattery::tests
