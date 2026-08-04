#include "in_game_settings_model.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "auto_cattery/level_up_reroll_data.hpp"

namespace autocattery::ui {
InGameSettingsModel::InGameSettingsModel(
    std::filesystem::path default_config,
    std::filesystem::path user_config,
    std::filesystem::path data_mod_root)
    : editor_({std::move(default_config), std::move(user_config)}),
      data_mod_root_(std::move(data_mod_root)) {}

Result<void> InGameSettingsModel::Reload() {
    const auto loaded = editor_.Load();
    if (!loaded) return {loaded.code, loaded.message};
    config_ = loaded.value;
    loaded_ = true;
    return {};
}

Result<void> InGameSettingsModel::Adjust(
    std::size_t page, std::size_t row, int direction) {
    auto pages = Pages();
    if (!loaded_ || page >= pages.size() ||
        row >= pages[page].fields.size()) {
        return {ErrorCode::ConfigInvalid, "setting selection is invalid"};
    }
    auto previous = config_;
    auto& field = pages[page].fields[row];
    if (auto* boolean = std::get_if<bool*>(&field.value)) {
        **boolean = !**boolean;
    } else if (auto* language = std::get_if<std::string*>(&field.value)) {
        **language = **language == "en-US" ? "zh-CN" : "en-US";
        config_.language = **language;
    } else if (auto* integer = std::get_if<std::size_t*>(&field.value)) {
        const auto signed_value = static_cast<double>(**integer) +
            (direction < 0 ? -field.step : field.step);
        **integer = static_cast<std::size_t>(std::clamp(
            signed_value, field.minimum, field.maximum));
    } else if (auto* decimal = std::get_if<double*>(&field.value)) {
        **decimal = std::clamp(
            **decimal + (direction < 0 ? -field.step : field.step),
            field.minimum, field.maximum);
    }
    config_.recommendation_marker.recommended_count =
        config_.combat_scoring.recommended_count;
    config_.safety = config_.execution_safety;
    if (!data_mod_root_.empty() &&
        config_.level_up.reroll_count != previous.level_up.reroll_count) {
        const auto written = WriteLevelUpRerollData(
            data_mod_root_, config_.level_up.reroll_count);
        if (!written) {
            config_ = std::move(previous);
            return {written.code, written.message};
        }
    }
    const auto saved = editor_.Save(config_);
    if (!saved) {
        if (!data_mod_root_.empty() &&
            config_.level_up.reroll_count != previous.level_up.reroll_count) {
            (void)WriteLevelUpRerollData(
                data_mod_root_, previous.level_up.reroll_count);
        }
        config_ = std::move(previous);
        return {saved.code, saved.message};
    }
    config_ = saved.value;
    return {};
}

Result<void> InGameSettingsModel::AdjustFlat(
    std::size_t index, int direction) {
    auto pages = Pages();
    for (std::size_t page = 0; page < pages.size(); ++page) {
        if (index < pages[page].fields.size()) {
            return Adjust(page, index, direction);
        }
        index -= pages[page].fields.size();
    }
    return {ErrorCode::ConfigInvalid, "setting selection is invalid"};
}

std::size_t InGameSettingsModel::PageCount() { return Pages().size(); }

std::string InGameSettingsModel::PageTitle(std::size_t page) {
    auto pages = Pages();
    if (page >= pages.size()) return IsEnglish() ? "Settings" : "设置";
    return std::string(IsEnglish() ? "Settings · " : "设置 · ") +
        pages[page].group + " · " +
        std::to_string(page + 1) + "/" + std::to_string(pages.size());
}

std::vector<std::string> InGameSettingsModel::Rows(std::size_t page) {
    auto pages = Pages();
    std::vector<std::string> rows;
    if (page >= pages.size()) return rows;
    for (const auto& field : pages[page].fields) rows.push_back(Format(field));
    return rows;
}

std::vector<std::string> InGameSettingsModel::AllRows() {
    std::vector<std::string> rows;
    auto pages = Pages();
    for (const auto& page : pages) {
        for (const auto& field : page.fields) rows.push_back(Format(field));
    }
    return rows;
}

std::string InGameSettingsModel::Format(const Field& field) const {
    std::ostringstream output;
    output << "<  " << field.label << (IsEnglish() ? ": " : "：");
    if (const auto* boolean = std::get_if<bool*>(&field.value)) {
        output << (**boolean
            ? (IsEnglish() ? "On" : "开启")
            : (IsEnglish() ? "Off" : "关闭"));
    } else if (const auto* language =
                   std::get_if<std::string*>(&field.value)) {
        output << (**language == "en-US" ? "English" : "中文");
    } else if (const auto* integer = std::get_if<std::size_t*>(&field.value)) {
        output << **integer;
    } else {
        output << std::fixed << std::setprecision(2)
               << **std::get_if<double*>(&field.value);
    }
    return output.str() + "  >";
}

std::vector<std::string> InGameSettingsModel::GroupTitles() const {
    return IsEnglish()
        ? std::vector<std::string>{
              "Combat Scoring", "Breeding & Classification",
              "Rooms, Safety & MOD"}
        : std::vector<std::string>{
              "战斗评分与推荐", "繁育评分与分类", "房间、安全与 MOD"};
}

bool InGameSettingsModel::IsEnglish() const noexcept {
    return config_.general.language == "en-US";
}

bool InGameSettingsModel::RequiresGameRestart(
    std::size_t index) const noexcept {
    return index == 47;
}

}  // namespace autocattery::ui
