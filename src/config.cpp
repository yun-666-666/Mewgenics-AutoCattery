#include "auto_cattery/config.hpp"

#include <fstream>
#include <set>

#include <nlohmann/json.hpp>

#include "auto_cattery/scoring/combat_scorer.hpp"
#include "auto_cattery/version.hpp"

namespace autocattery {
namespace {

using Json = nlohmann::json;

Json SafeDefaults() {
    return {
        {"schema_version", 1},
        {"mod_enabled", true},
        {"safe_mode", true},
        {"log_level", "info"},
        {"language", "zh-CN"},
        {"ui", {
            {"house_button_enabled", true},
            {"embark_button_enabled", true},
            {"show_debug_overlay", false}
        }},
        {"safety", {
            {"require_preview_before_destructive_actions", true},
            {"create_backup_before_apply", true},
            {"abort_on_unknown_game_build", true}
        }},
        {"combat_scoring", {
            {"version", 1},
            {"recommended_count", 8},
            {"minimum_score", 0.0},
            {"minimum_known_stats", 7},
            {"exclude_kittens", true},
            {"exclude_injured", false},
            {"require_confirmed_eligibility", true},
            {"missing_stat_penalty", 0.0},
            {"active_ability_default_weight", 0.0},
            {"passive_default_weight", 0.0},
            {"disorder_default_penalty", 0.0},
            {"injury_penalty", 0.0},
            {"stat_weights", {
                {"strength", 1.0},
                {"dexterity", 1.0},
                {"constitution", 1.0},
                {"intelligence", 1.0},
                {"speed", 1.0},
                {"charisma", 1.0},
                {"luck", 1.0}
            }},
            {"active_ability_overrides", Json::object()},
            {"passive_overrides", Json::object()},
            {"disorder_overrides", Json::object()}
        }}
    };
}

Result<Json> ReadJsonIfPresent(const std::filesystem::path& path) {
    if (path.empty() || !std::filesystem::exists(path)) {
        return {Json::object(), ErrorCode::Ok, {}};
    }

    std::ifstream stream(path);
    if (!stream) {
        return {{}, ErrorCode::ConfigInvalid, "cannot open configuration file"};
    }

    try {
        Json parsed;
        stream >> parsed;
        if (!parsed.is_object()) {
            return {{}, ErrorCode::ConfigInvalid, "configuration root must be an object"};
        }
        return {std::move(parsed), ErrorCode::Ok, {}};
    } catch (const Json::exception& error) {
        return {{}, ErrorCode::ConfigInvalid, error.what()};
    }
}

template<class T>
void AssignIfPresent(const Json& object, const char* key, T& destination) {
    if (const auto it = object.find(key); it != object.end()) {
        destination = it->get<T>();
    }
}

void AssignWeights(
    const Json& object,
    std::array<double, snapshot::kStatCount>& destination) {
    static constexpr std::array<const char*, snapshot::kStatCount> keys{
        "strength",
        "dexterity",
        "constitution",
        "intelligence",
        "speed",
        "charisma",
        "luck"
    };
    for (std::size_t index = 0; index < keys.size(); ++index) {
        AssignIfPresent(object, keys[index], destination[index]);
    }
}

void AssignOverrides(
    const Json& object,
    std::unordered_map<std::string, double>& destination) {
    destination.clear();
    for (const auto& [key, value] : object.items()) {
        destination.emplace(key, value.get<double>());
    }
}

Result<void> Validate(const Json& value) {
    try {
        if (!value.at("schema_version").is_number_integer()) {
            return {ErrorCode::ConfigInvalid, "schema_version must be an integer"};
        }

        static const std::set<std::string> levels{
            "trace", "debug", "info", "warn", "error"
        };
        const auto level = value.at("log_level").get<std::string>();
        if (!levels.contains(level)) {
            return {ErrorCode::ConfigInvalid, "log_level is not supported"};
        }

        if (!value.at("safety")
                 .at("require_preview_before_destructive_actions")
                 .get<bool>()) {
            return {
                ErrorCode::ConfigInvalid,
                "preview-before-destructive-actions cannot be disabled"
            };
        }
        if (!value.at("safety").at("create_backup_before_apply").get<bool>()) {
            return {
                ErrorCode::ConfigInvalid,
                "backup-before-apply cannot be disabled"
            };
        }
    } catch (const Json::exception& error) {
        return {ErrorCode::ConfigInvalid, error.what()};
    }
    return {};
}

}  // namespace

Result<Config> LoadConfig(
    const std::filesystem::path& default_path,
    const std::filesystem::path& user_path) {
    auto merged = SafeDefaults();

    const auto defaults = ReadJsonIfPresent(default_path);
    if (!defaults) {
        return {{}, defaults.code, "default_config.json: " + defaults.message};
    }
    merged.merge_patch(defaults.value);

    const auto user = ReadJsonIfPresent(user_path);
    if (!user) {
        return {{}, user.code, "user_config.json: " + user.message};
    }
    merged.merge_patch(user.value);

    const auto validation = Validate(merged);
    if (!validation) {
        return {{}, validation.code, validation.message};
    }

    Config result;
    try {
        AssignIfPresent(merged, "schema_version", result.schema_version);
        AssignIfPresent(merged, "mod_enabled", result.mod_enabled);
        AssignIfPresent(merged, "safe_mode", result.safe_mode);
        AssignIfPresent(merged, "log_level", result.log_level);
        AssignIfPresent(merged, "language", result.language);

        const auto& ui = merged.at("ui");
        AssignIfPresent(ui, "house_button_enabled", result.ui.house_button_enabled);
        AssignIfPresent(ui, "embark_button_enabled", result.ui.embark_button_enabled);
        AssignIfPresent(ui, "show_debug_overlay", result.ui.show_debug_overlay);

        const auto& safety = merged.at("safety");
        AssignIfPresent(
            safety,
            "require_preview_before_destructive_actions",
            result.safety.require_preview_before_destructive_actions);
        AssignIfPresent(
            safety,
            "create_backup_before_apply",
            result.safety.create_backup_before_apply);
        AssignIfPresent(
            safety,
            "abort_on_unknown_game_build",
            result.safety.abort_on_unknown_game_build);

        const auto& combat = merged.at("combat_scoring");
        AssignIfPresent(
            combat,
            "version",
            result.combat_scoring.version);
        AssignIfPresent(
            combat,
            "recommended_count",
            result.combat_scoring.recommended_count);
        AssignIfPresent(
            combat,
            "minimum_score",
            result.combat_scoring.minimum_score);
        AssignIfPresent(
            combat,
            "minimum_known_stats",
            result.combat_scoring.minimum_known_stats);
        AssignIfPresent(
            combat,
            "exclude_kittens",
            result.combat_scoring.exclude_kittens);
        AssignIfPresent(
            combat,
            "exclude_injured",
            result.combat_scoring.exclude_injured);
        AssignIfPresent(
            combat,
            "require_confirmed_eligibility",
            result.combat_scoring.require_confirmed_eligibility);
        AssignIfPresent(
            combat,
            "missing_stat_penalty",
            result.combat_scoring.missing_stat_penalty);
        AssignIfPresent(
            combat,
            "active_ability_default_weight",
            result.combat_scoring.active_ability_default_weight);
        AssignIfPresent(
            combat,
            "passive_default_weight",
            result.combat_scoring.passive_default_weight);
        AssignIfPresent(
            combat,
            "disorder_default_penalty",
            result.combat_scoring.disorder_default_penalty);
        AssignIfPresent(
            combat,
            "injury_penalty",
            result.combat_scoring.injury_penalty);
        AssignWeights(
            combat.at("stat_weights"),
            result.combat_scoring.stat_weights);
        AssignOverrides(
            combat.at("active_ability_overrides"),
            result.combat_scoring.active_ability_overrides);
        AssignOverrides(
            combat.at("passive_overrides"),
            result.combat_scoring.passive_overrides);
        AssignOverrides(
            combat.at("disorder_overrides"),
            result.combat_scoring.disorder_overrides);
    } catch (const Json::exception& error) {
        return {{}, ErrorCode::ConfigInvalid, error.what()};
    }

    const auto scoring_validation = scoring::Validate(result.combat_scoring);
    if (!scoring_validation) {
        return {
            {},
            scoring_validation.code,
            "combat_scoring: " + scoring_validation.message
        };
    }
    result.force_read_only = result.schema_version > kSupportedConfigSchema;
    return {std::move(result), ErrorCode::Ok, {}};
}

}  // namespace autocattery
