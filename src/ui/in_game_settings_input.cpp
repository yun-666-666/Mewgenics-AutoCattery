#include "in_game_settings_model.hpp"

#include <charconv>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <utility>

#include "auto_cattery/level_up_reroll_data.hpp"

namespace autocattery::ui {

std::optional<InGameSettingsModel::Field>
InGameSettingsModel::FlatField(std::size_t index) {
    auto pages = Pages();
    for (const auto& page : pages) {
        if (index < page.fields.size()) return page.fields[index];
        index -= page.fields.size();
    }
    return std::nullopt;
}

std::optional<std::string> InGameSettingsModel::DirectValue(
    std::size_t index) {
    if (!loaded_) return std::nullopt;
    const auto field = FlatField(index);
    if (!field || std::holds_alternative<bool*>(field->value) ||
        std::holds_alternative<std::string*>(field->value)) {
        return std::nullopt;
    }
    if (const auto* integer = std::get_if<std::size_t*>(&field->value)) {
        return std::to_string(**integer);
    }
    std::ostringstream output;
    output << std::fixed << std::setprecision(2)
           << **std::get_if<double*>(&field->value);
    return output.str();
}

Result<void> InGameSettingsModel::SetFlatValue(
    std::size_t index, std::string_view text) {
    if (!loaded_) {
        return {ErrorCode::ConfigInvalid, "settings are not loaded"};
    }
    const auto field = FlatField(index);
    if (!field || std::holds_alternative<bool*>(field->value) ||
        std::holds_alternative<std::string*>(field->value)) {
        return {ErrorCode::ConfigInvalid,
                "this setting does not accept numeric input"};
    }
    auto previous = config_;
    if (auto* integer = std::get_if<std::size_t*>(&field->value)) {
        std::size_t value{};
        const auto parsed = std::from_chars(
            text.data(), text.data() + text.size(), value);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
            static_cast<double>(value) < field->minimum ||
            static_cast<double>(value) > field->maximum) {
            return {ErrorCode::ConfigInvalid, "integer input is invalid"};
        }
        **integer = value;
    } else {
        double value{};
        const auto parsed = std::from_chars(
            text.data(), text.data() + text.size(), value);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
            !std::isfinite(value) || value < field->minimum ||
            value > field->maximum) {
            return {ErrorCode::ConfigInvalid, "decimal input is invalid"};
        }
        **std::get_if<double*>(&field->value) = value;
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

}  // namespace autocattery::ui
