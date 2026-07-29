#include "auto_cattery/settings_service.hpp"

#include <algorithm>
#include <array>

namespace autocattery {
namespace {

constexpr double kWeightStep = 0.25;

std::optional<std::size_t> StatIndex(std::string_view name) {
    static constexpr std::array<std::string_view, snapshot::kStatCount> names{
        "strength",
        "dexterity",
        "constitution",
        "intelligence",
        "speed",
        "charisma",
        "luck"
    };
    for (std::size_t index = 0; index < names.size(); ++index) {
        if (name == names[index]) {
            return index;
        }
    }
    return std::nullopt;
}

bool RequestedBoolean(bool current, int direction, bool toggle) {
    return toggle ? !current : direction > 0;
}

}  // namespace

Result<void> SettingsService::AdjustControl(
    std::string_view key,
    int direction,
    bool toggle,
    bool confirm_single_click_enable,
    workflow::WorkflowState state) {
    const auto config = Current();
    if (key == "combat_scoring.recommended_count") {
        const auto current = config.combat_scoring.recommended_count;
        SimpleSettingsUpdate update;
        update.recommended_count = direction < 0
            ? current - (current > 1 ? 1 : 0)
            : std::min<std::size_t>(100, current + 1);
        return ApplySimple(update, state);
    }
    if (key == "combat_scoring.exclude_injured") {
        SimpleSettingsUpdate update;
        update.exclude_injured = RequestedBoolean(
            config.combat_scoring.exclude_injured,
            direction,
            toggle);
        return ApplySimple(update, state);
    }
    if (key == "classification.minimum_general_reserve") {
        const auto current = config.classification.minimum_general_reserve;
        SimpleSettingsUpdate update;
        update.minimum_general_reserve = direction < 0
            ? current - (current > 0 ? 1 : 0)
            : std::min<std::size_t>(10000, current + 1);
        return ApplySimple(update, state);
    }
    if (key == "room_planning.allow_soft_overflow") {
        SimpleSettingsUpdate update;
        update.allow_soft_overflow = RequestedBoolean(
            config.room_planning.allow_soft_overflow,
            direction,
            toggle);
        return ApplySimple(update, state);
    }
    if (key.starts_with("combat_scoring.stat_weights.")) {
        constexpr std::string_view prefix = "combat_scoring.stat_weights.";
        const auto index = StatIndex(key.substr(prefix.size()));
        if (!index) {
            return {ErrorCode::ConfigInvalid, "combat stat setting is invalid"};
        }
        auto weights = config.combat_scoring.stat_weights;
        weights[*index] = std::clamp(
            weights[*index] + (direction < 0 ? -kWeightStep : kWeightStep),
            -10000.0,
            10000.0);
        AdvancedSettingsUpdate update;
        update.combat_stat_weights = weights;
        return ApplyAdvanced(update, state);
    }
    if (key.starts_with("breeding_scoring.stat_weights.")) {
        constexpr std::string_view prefix = "breeding_scoring.stat_weights.";
        const auto index = StatIndex(key.substr(prefix.size()));
        if (!index) {
            return {ErrorCode::ConfigInvalid, "breeding stat setting is invalid"};
        }
        auto weights = config.breeding_scoring.stat_weights;
        weights[*index] = std::clamp(
            weights[*index] + (direction < 0 ? -kWeightStep : kWeightStep),
            -10000.0,
            10000.0);
        AdvancedSettingsUpdate update;
        update.breeding_stat_weights = weights;
        return ApplyAdvanced(update, state);
    }
    if (key == "execution_safety.read_only_mode") {
        SafetySettingsUpdate update;
        update.read_only_mode = RequestedBoolean(
            config.execution_safety.read_only_mode,
            direction,
            toggle);
        return ApplySafety(update, state);
    }
    if (key == "execution_safety.create_backup_before_apply") {
        SafetySettingsUpdate update;
        update.create_backup_before_apply = RequestedBoolean(
            config.execution_safety.create_backup_before_apply,
            direction,
            toggle);
        return ApplySafety(update, state);
    }
    if (key == "execution_safety.single_click_execute") {
        SafetySettingsUpdate update;
        update.single_click_execute = RequestedBoolean(
            config.execution_safety.single_click_execute,
            direction,
            toggle);
        update.confirm_single_click_enable = confirm_single_click_enable;
        return ApplySafety(update, state);
    }
    return {ErrorCode::ConfigInvalid, "settings control is read-only or unknown"};
}

}  // namespace autocattery
