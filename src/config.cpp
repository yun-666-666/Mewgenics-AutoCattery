#include "auto_cattery/config.hpp"

#include <fstream>
#include <set>

#include <nlohmann/json.hpp>

#include "auto_cattery/breeding/breeding_scorer.hpp"
#include "auto_cattery/classification/classifier.hpp"
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
        {"execution", {
            {"real_write_adapter_enabled", false},
            {"cull_enabled", false},
            {"require_quiescent_backup", true}
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
        }},
        {"breeding_scoring", {
            {"version", 1},
            {"core_breeders", 4},
            {"reserve_breeders", 4},
            {"minimum_score", 0.0},
            {"minimum_known_stats", 7},
            {"require_confirmed_eligibility", true},
            {"missing_stat_penalty", 0.0},
            {"active_ability_default_weight", 0.0},
            {"passive_default_weight", 0.0},
            {"disorder_default_penalty", 0.0},
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
        }},
        {"classification", {
            {"version", 1},
            {"combat_priority_over_breeding", false},
            {"minimum_combat_pool", 8},
            {"minimum_breeding_pool", 8},
            {"minimum_general_reserve", 4},
            {"never_cull_if_data_confidence_below", 0.85}
        }},
        {"protection", {
            {"version", 1},
            {"sidecar_file", "protection.json"},
            {"protect_unknown_native_state", true},
            {"require_stable_identity_for_sidecar", true}
        }},
        {"room_planning", {
            {"version", 1},
            {"default_soft_capacity", 4},
            {"allow_soft_overflow", true},
            {"max_soft_overflow_per_room", 2},
            {"never_exceed_known_hard_capacity", true},
            {"prefer_single_combat_staging_room", true},
            {"keep_breeding_pairs_together", true},
            {"avoid_inbreeding_pairs", true},
            {"keep_kittens_separate_when_possible", true},
            {"allow_partial_plan", true}
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
        if (value.at("execution")
                .at("real_write_adapter_enabled")
                .get<bool>() ||
            value.at("execution").at("cull_enabled").get<bool>() ||
            !value.at("execution")
                 .at("require_quiescent_backup")
                 .get<bool>()) {
            return {
                ErrorCode::ConfigInvalid,
                "unverified execution cannot be enabled and quiescent "
                "backup cannot be disabled"
            };
        }
        if (!value.at("protection")
                 .at("protect_unknown_native_state")
                 .get<bool>() ||
            !value.at("protection")
                 .at("require_stable_identity_for_sidecar")
                 .get<bool>()) {
            return {
                ErrorCode::ConfigInvalid,
                "protection fail-closed rules cannot be disabled"
            };
        }
        if (!value.at("room_planning")
                 .at("never_exceed_known_hard_capacity")
                 .get<bool>() ||
            !value.at("room_planning")
                 .at("allow_partial_plan")
                 .get<bool>()) {
            return {
                ErrorCode::ConfigInvalid,
                "room planning hard-capacity and partial-plan safety rules "
                "cannot be disabled"
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

        const auto& execution = merged.at("execution");
        AssignIfPresent(
            execution,
            "real_write_adapter_enabled",
            result.execution.real_write_adapter_enabled);
        AssignIfPresent(
            execution,
            "cull_enabled",
            result.execution.cull_enabled);
        AssignIfPresent(
            execution,
            "require_quiescent_backup",
            result.execution.require_quiescent_backup);

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

        const auto& breeding = merged.at("breeding_scoring");
        AssignIfPresent(
            breeding, "version", result.breeding_scoring.version);
        AssignIfPresent(
            breeding, "core_breeders", result.breeding_scoring.core_breeders);
        AssignIfPresent(
            breeding,
            "reserve_breeders",
            result.breeding_scoring.reserve_breeders);
        AssignIfPresent(
            breeding, "minimum_score", result.breeding_scoring.minimum_score);
        AssignIfPresent(
            breeding,
            "minimum_known_stats",
            result.breeding_scoring.minimum_known_stats);
        AssignIfPresent(
            breeding,
            "require_confirmed_eligibility",
            result.breeding_scoring.require_confirmed_eligibility);
        AssignIfPresent(
            breeding,
            "missing_stat_penalty",
            result.breeding_scoring.missing_stat_penalty);
        AssignIfPresent(
            breeding,
            "active_ability_default_weight",
            result.breeding_scoring.active_ability_default_weight);
        AssignIfPresent(
            breeding,
            "passive_default_weight",
            result.breeding_scoring.passive_default_weight);
        AssignIfPresent(
            breeding,
            "disorder_default_penalty",
            result.breeding_scoring.disorder_default_penalty);
        AssignWeights(
            breeding.at("stat_weights"),
            result.breeding_scoring.stat_weights);
        AssignOverrides(
            breeding.at("active_ability_overrides"),
            result.breeding_scoring.active_ability_overrides);
        AssignOverrides(
            breeding.at("passive_overrides"),
            result.breeding_scoring.passive_overrides);
        AssignOverrides(
            breeding.at("disorder_overrides"),
            result.breeding_scoring.disorder_overrides);

        const auto& classification = merged.at("classification");
        AssignIfPresent(
            classification,
            "version",
            result.classification.version);
        AssignIfPresent(
            classification,
            "combat_priority_over_breeding",
            result.classification.combat_priority_over_breeding);
        AssignIfPresent(
            classification,
            "minimum_combat_pool",
            result.classification.minimum_combat_pool);
        AssignIfPresent(
            classification,
            "minimum_breeding_pool",
            result.classification.minimum_breeding_pool);
        AssignIfPresent(
            classification,
            "minimum_general_reserve",
            result.classification.minimum_general_reserve);
        AssignIfPresent(
            classification,
            "never_cull_if_data_confidence_below",
            result.classification.never_cull_if_data_confidence_below);

        const auto& protection = merged.at("protection");
        AssignIfPresent(
            protection, "version", result.protection.version);
        AssignIfPresent(
            protection, "sidecar_file", result.protection.sidecar_file);
        AssignIfPresent(
            protection,
            "protect_unknown_native_state",
            result.protection.protect_unknown_native_state);
        AssignIfPresent(
            protection,
            "require_stable_identity_for_sidecar",
            result.protection.require_stable_identity_for_sidecar);
        if (result.protection.version != 1 ||
            result.protection.sidecar_file.empty()) {
            return {
                {},
                ErrorCode::ConfigInvalid,
                "protection configuration version or sidecar filename is "
                "invalid"
            };
        }

        const auto& room_planning = merged.at("room_planning");
        AssignIfPresent(
            room_planning,
            "version",
            result.room_planning.version);
        AssignIfPresent(
            room_planning,
            "default_soft_capacity",
            result.room_planning.default_soft_capacity);
        AssignIfPresent(
            room_planning,
            "allow_soft_overflow",
            result.room_planning.allow_soft_overflow);
        AssignIfPresent(
            room_planning,
            "max_soft_overflow_per_room",
            result.room_planning.max_soft_overflow_per_room);
        AssignIfPresent(
            room_planning,
            "never_exceed_known_hard_capacity",
            result.room_planning.never_exceed_known_hard_capacity);
        AssignIfPresent(
            room_planning,
            "prefer_single_combat_staging_room",
            result.room_planning.prefer_single_combat_staging_room);
        AssignIfPresent(
            room_planning,
            "keep_breeding_pairs_together",
            result.room_planning.keep_breeding_pairs_together);
        AssignIfPresent(
            room_planning,
            "avoid_inbreeding_pairs",
            result.room_planning.avoid_inbreeding_pairs);
        AssignIfPresent(
            room_planning,
            "keep_kittens_separate_when_possible",
            result.room_planning.keep_kittens_separate_when_possible);
        AssignIfPresent(
            room_planning,
            "allow_partial_plan",
            result.room_planning.allow_partial_plan);
        if (result.room_planning.version != 1 ||
            result.room_planning.default_soft_capacity == 0 ||
            !result.room_planning.never_exceed_known_hard_capacity ||
            !result.room_planning.allow_partial_plan) {
            return {
                {},
                ErrorCode::ConfigInvalid,
                "room planning configuration is invalid"
            };
        }
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
    const auto breeding_validation =
        breeding::Validate(result.breeding_scoring);
    if (!breeding_validation) {
        return {
            {},
            breeding_validation.code,
            "breeding_scoring: " + breeding_validation.message
        };
    }
    const auto classification_validation =
        classification::Validate(result.classification);
    if (!classification_validation) {
        return {
            {},
            classification_validation.code,
            "classification: " + classification_validation.message
        };
    }
    result.force_read_only = result.schema_version > kSupportedConfigSchema;
    return {std::move(result), ErrorCode::Ok, {}};
}

}  // namespace autocattery
