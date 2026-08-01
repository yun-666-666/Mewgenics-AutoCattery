#include "in_game_settings_model.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace autocattery::ui {
InGameSettingsModel::InGameSettingsModel(
    std::filesystem::path default_config,
    std::filesystem::path user_config)
    : editor_({std::move(default_config), std::move(user_config)}) {}

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
    const auto saved = editor_.Save(config_);
    if (!saved) {
        config_ = std::move(previous);
        return {saved.code, saved.message};
    }
    config_ = saved.value;
    return {};
}

std::size_t InGameSettingsModel::PageCount() { return Pages().size(); }

std::string InGameSettingsModel::PageTitle(std::size_t page) {
    auto pages = Pages();
    if (page >= pages.size()) return "设置";
    return std::string("设置 · ") + pages[page].group + " · " +
        std::to_string(page + 1) + "/" + std::to_string(pages.size());
}

std::vector<std::string> InGameSettingsModel::Rows(std::size_t page) {
    auto pages = Pages();
    std::vector<std::string> rows;
    if (page >= pages.size()) return rows;
    for (const auto& field : pages[page].fields) rows.push_back(Format(field));
    return rows;
}

std::string InGameSettingsModel::Format(const Field& field) const {
    std::ostringstream output;
    output << "<  " << field.label << "：";
    if (const auto* boolean = std::get_if<bool*>(&field.value)) {
        output << (**boolean ? "开启" : "关闭");
    } else if (const auto* integer = std::get_if<std::size_t*>(&field.value)) {
        output << **integer;
    } else {
        output << std::fixed << std::setprecision(2)
               << **std::get_if<double*>(&field.value);
    }
    return output.str() + "  >";
}

}  // namespace autocattery::ui
