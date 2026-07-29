#include "auto_cattery/settings_service.hpp"

#include <array>
#include <iomanip>
#include <sstream>

namespace autocattery {
namespace {

std::string BoolText(bool value) {
    return value ? "true" : "false";
}

std::string SizeText(std::size_t value) {
    return std::to_string(value);
}

std::string WeightText(double value) {
    std::ostringstream text;
    text << std::fixed << std::setprecision(2) << value;
    return text.str();
}

Result<void> ToVoid(const ConfigReloadResult& result) {
    if (result.status == ConfigReloadStatus::Applied ||
        result.status == ConfigReloadStatus::Unchanged) {
        return {};
    }
    return {
        result.code == ErrorCode::Ok ? ErrorCode::ConfigInvalid : result.code,
        result.message
    };
}

}  // namespace

SettingsService::SettingsService(RuntimeConfigService& runtime)
    : runtime_(runtime) {}

Config SettingsService::Current() const {
    return runtime_.Current();
}

std::vector<SettingsPage> SettingsService::Pages() const {
    const auto config = runtime_.Current();
    std::vector<SettingsPage> pages{
        {
            SettingsPageKind::Simple,
            "Simple",
            {
                {
                    "combat_scoring.recommended_count",
                    "Recommended cats",
                    SizeText(config.combat_scoring.recommended_count),
                    false,
                    false
                },
                {
                    "combat_scoring.exclude_injured",
                    "Exclude injured",
                    BoolText(config.combat_scoring.exclude_injured),
                    false,
                    false
                },
                {
                    "classification.minimum_general_reserve",
                    "Minimum reserve",
                    SizeText(config.classification.minimum_general_reserve),
                    true,
                    false
                },
                {
                    "room_planning.allow_soft_overflow",
                    "Allow soft overflow",
                    BoolText(config.room_planning.allow_soft_overflow),
                    true,
                    false
                }
            }
        },
        {
            SettingsPageKind::Advanced,
            "Advanced",
            {}
        },
        {
            SettingsPageKind::Safety,
            "Safety",
            {
                {
                    "execution_safety.read_only_mode",
                    "Read only",
                    BoolText(config.execution_safety.read_only_mode),
                    true,
                    false
                },
                {
                    "execution_safety.create_backup_before_apply",
                    "Create backup",
                    BoolText(config.execution_safety.create_backup_before_apply),
                    true,
                    false
                },
                {
                    "execution_safety.single_click_execute",
                    "Single-click execute",
                    BoolText(config.execution_safety.single_click_execute),
                    true,
                    false
                },
                {
                    "execution_safety.require_preview_before_destructive_actions",
                    "Preview required",
                    BoolText(config.execution_safety
                        .require_preview_before_destructive_actions),
                    true,
                    true
                },
                {
                    "recommendation_marker.never_auto_select",
                    "Never auto-select",
                    BoolText(config.recommendation_marker.never_auto_select),
                    true,
                    true
                }
            }
        }
    };
    static constexpr std::array<const char*, snapshot::kStatCount> stat_keys{
        "strength",
        "dexterity",
        "constitution",
        "intelligence",
        "speed",
        "charisma",
        "luck"
    };
    static constexpr std::array<const char*, snapshot::kStatCount> labels{
        "Strength",
        "Dexterity",
        "Constitution",
        "Intelligence",
        "Speed",
        "Charisma",
        "Luck"
    };
    auto& advanced = pages[1].controls;
    advanced.reserve(snapshot::kStatCount * 2);
    for (std::size_t index = 0; index < stat_keys.size(); ++index) {
        advanced.push_back({
            std::string("combat_scoring.stat_weights.") + stat_keys[index],
            std::string("Combat ") + labels[index],
            WeightText(config.combat_scoring.stat_weights[index]),
            false,
            false
        });
    }
    for (std::size_t index = 0; index < stat_keys.size(); ++index) {
        advanced.push_back({
            std::string("breeding_scoring.stat_weights.") + stat_keys[index],
            std::string("Breeding ") + labels[index],
            WeightText(config.breeding_scoring.stat_weights[index]),
            false,
            false
        });
    }
    return pages;
}

Result<void> SettingsService::ApplySimple(
    const SimpleSettingsUpdate& update,
    workflow::WorkflowState state) {
    SessionConfigOverride session;
    session.combat_recommended_count = update.recommended_count;
    session.combat_exclude_injured = update.exclude_injured;
    session.minimum_general_reserve = update.minimum_general_reserve;
    session.allow_soft_overflow = update.allow_soft_overflow;
    return ToVoid(runtime_.ApplySessionOverride(session, state));
}

Result<void> SettingsService::ApplyAdvanced(
    const AdvancedSettingsUpdate& update,
    workflow::WorkflowState state) {
    SessionConfigOverride session;
    session.combat_stat_weights = update.combat_stat_weights;
    session.breeding_stat_weights = update.breeding_stat_weights;
    return ToVoid(runtime_.ApplySessionOverride(session, state));
}

Result<void> SettingsService::ApplySafety(
    const SafetySettingsUpdate& update,
    workflow::WorkflowState state) {
    if (update.single_click_execute && *update.single_click_execute &&
        !update.confirm_single_click_enable) {
        return {
            ErrorCode::ConfigInvalid,
            "single-click execution requires explicit confirmation"
        };
    }
    SessionConfigOverride session;
    session.read_only_mode = update.read_only_mode;
    session.single_click_execute = update.single_click_execute;
    session.create_backup_before_apply = update.create_backup_before_apply;
    return ToVoid(runtime_.ApplySessionOverride(session, state));
}

}  // namespace autocattery
