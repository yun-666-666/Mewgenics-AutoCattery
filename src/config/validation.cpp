#include "config_json.hpp"

#include <cmath>
#include <filesystem>
#include <set>
#include <string_view>

#include "auto_cattery/snapshot/domain.hpp"

namespace autocattery::config_detail {
namespace {

constexpr double kMinimumWeight = -10000.0;
constexpr double kMaximumWeight = 10000.0;
constexpr std::size_t kMaximumPoolSize = 10000;
constexpr std::size_t kMaximumRoomPreference = 1000;
constexpr std::size_t kMaximumOverrides = 512;

bool ReasonableFinite(double value) {
    return std::isfinite(value) &&
           value >= kMinimumWeight && value <= kMaximumWeight;
}

Result<void> ValidateModuleVersion(
    const Json& module,
    std::string_view path) {
    if (!module.is_object()) {
        return {ErrorCode::ConfigInvalid, std::string(path) + " must be an object"};
    }
    if (module.at("version").get<std::uint32_t>() != 1) {
        return {ErrorCode::ConfigInvalid, std::string(path) + ".version must be 1"};
    }
    return {};
}

Result<void> ValidateWeightObject(
    const Json& object,
    std::string path,
    bool stat_weights) {
    if (!object.is_object()) {
        return {ErrorCode::ConfigInvalid, path + " must be an object"};
    }
    if (object.size() > kMaximumOverrides) {
        return {ErrorCode::ConfigInvalid, path + " has too many entries"};
    }
    static const std::set<std::string> stat_names{
        "strength",
        "dexterity",
        "constitution",
        "intelligence",
        "speed",
        "charisma",
        "luck"
    };
    for (const auto& [key, value] : object.items()) {
        if (key.empty() || key.size() > 128) {
            return {ErrorCode::ConfigInvalid, path + " has an invalid key"};
        }
        if (stat_weights && !stat_names.contains(key)) {
            return {ErrorCode::ConfigInvalid, path + "." + key + " is not a known stat"};
        }
        if (!value.is_number()) {
            return {ErrorCode::ConfigInvalid, path + "." + key + " must be numeric"};
        }
        if (!ReasonableFinite(value.get<double>())) {
            return {
                ErrorCode::ConfigInvalid,
                path + "." + key +
                    " must be finite and between -10000 and 10000"
            };
        }
    }
    if (stat_weights && object.size() != snapshot::kStatCount) {
        return {ErrorCode::ConfigInvalid, path + " must contain all seven confirmed stats"};
    }
    return {};
}

Result<void> ValidateScoring(
    const Json& module,
    std::string path,
    bool combat) {
    const auto version = ValidateModuleVersion(module, path);
    if (!version) {
        return version;
    }
    if (combat) {
        const auto count = module.at("recommended_count").get<std::size_t>();
        if (count < 1 || count > 100) {
            return {
                ErrorCode::ConfigInvalid,
                path + ".recommended_count must be between 1 and 100"
            };
        }
    } else {
        for (const char* key : {"core_breeders", "reserve_breeders"}) {
            if (module.at(key).get<std::size_t>() > kMaximumPoolSize) {
                return {
                    ErrorCode::ConfigInvalid,
                    path + "." + key + " must not exceed 10000"
                };
            }
        }
    }
    if (module.at("minimum_known_stats").get<std::size_t>() >
        snapshot::kStatCount) {
        return {
            ErrorCode::ConfigInvalid,
            path + ".minimum_known_stats must not exceed 7"
        };
    }
    for (const char* key : {
             "minimum_score",
             "missing_stat_penalty",
             "active_ability_default_weight",
             "passive_default_weight",
             "disorder_default_penalty"}) {
        if (!ReasonableFinite(module.at(key).get<double>())) {
            return {
                ErrorCode::ConfigInvalid,
                path + "." + key +
                    " must be finite and between -10000 and 10000"
            };
        }
    }
    if (combat && !ReasonableFinite(module.at("injury_penalty").get<double>())) {
        return {
            ErrorCode::ConfigInvalid,
            path + ".injury_penalty must be finite and between -10000 and 10000"
        };
    }
    auto result = ValidateWeightObject(
        module.at("stat_weights"),
        path + ".stat_weights",
        true);
    if (!result) {
        return result;
    }
    for (const char* key : {
             "active_ability_overrides",
             "passive_overrides",
             "disorder_overrides"}) {
        result = ValidateWeightObject(
            module.at(key),
            path + "." + key,
            false);
        if (!result) {
            return result;
        }
    }
    return {};
}

Result<void> ValidateGeneralAndUi(const Json& value) {
    auto result = ValidateModuleVersion(value.at("general"), "general");
    if (!result) {
        return result;
    }
    result = ValidateModuleVersion(value.at("ui"), "ui");
    if (!result) {
        return result;
    }
    static const std::set<std::string> levels{
        "trace", "debug", "info", "warn", "error"
    };
    const auto level = value.at("general").at("log_level").get<std::string>();
    if (!levels.contains(level)) {
        return {ErrorCode::ConfigInvalid, "general.log_level is not supported"};
    }
    const auto language = value.at("general").at("language").get<std::string>();
    if (language.empty() || language.size() > 32) {
        return {ErrorCode::ConfigInvalid, "general.language is invalid"};
    }
    (void)value.at("general").at("mod_enabled").get<bool>();
    (void)value.at("general").at("safe_mode").get<bool>();
    (void)value.at("ui").at("house_button_enabled").get<bool>();
    (void)value.at("ui").at("embark_button_enabled").get<bool>();
    (void)value.at("ui").at("show_debug_overlay").get<bool>();
    return {};
}

Result<void> ValidateSafety(const Json& value) {
    const auto& safety = value.at("execution_safety");
    const auto version = ValidateModuleVersion(safety, "execution_safety");
    if (!version) {
        return version;
    }
    if (!safety.at("require_preview_before_destructive_actions").get<bool>()) {
        return {
            ErrorCode::ConfigInvalid,
            "execution_safety.require_preview_before_destructive_actions cannot be disabled"
        };
    }
    if (!safety.at("abort_on_unknown_game_build").get<bool>()) {
        return {
            ErrorCode::ConfigInvalid,
            "execution_safety.abort_on_unknown_game_build cannot be disabled"
        };
    }
    (void)safety.at("create_backup_before_apply").get<bool>();
    (void)safety.at("read_only_mode").get<bool>();
    (void)safety.at("single_click_execute").get<bool>();

    const auto& execution = value.at("execution");
    if (execution.at("real_write_adapter_enabled").get<bool>() ||
        execution.at("cull_enabled").get<bool>() ||
        !execution.at("require_quiescent_backup").get<bool>()) {
        return {
            ErrorCode::ConfigInvalid,
            "unverified execution cannot be enabled and quiescent backup cannot be disabled"
        };
    }
    const auto ttl = value.at("workflow").at("preview_ttl_seconds")
        .get<std::uint32_t>();
    if (ttl < 10 || ttl > 600) {
        return {
            ErrorCode::ConfigInvalid,
            "workflow.preview_ttl_seconds must be between 10 and 600"
        };
    }
    return {};
}

Result<void> ValidateProtectionAndPlanning(const Json& value) {
    const auto& protection = value.at("protection");
    auto result = ValidateModuleVersion(protection, "protection");
    if (!result) {
        return result;
    }
    if (!protection.at("protect_unknown_native_state").get<bool>() ||
        !protection.at("require_stable_identity_for_sidecar").get<bool>()) {
        return {
            ErrorCode::ConfigInvalid,
            "protection fail-closed rules cannot be disabled"
        };
    }
    const auto sidecar = protection.at("sidecar_file").get<std::string>();
    const std::filesystem::path sidecar_path(sidecar);
    if (sidecar.empty() || sidecar.size() > 128 ||
        sidecar_path.is_absolute() || sidecar_path.has_parent_path()) {
        return {ErrorCode::ConfigInvalid, "protection.sidecar_file must be a filename"};
    }

    const auto& classification = value.at("classification");
    result = ValidateModuleVersion(classification, "classification");
    if (!result) {
        return result;
    }
    for (const char* key : {
             "minimum_combat_pool",
             "minimum_breeding_pool",
             "minimum_general_reserve"}) {
        if (classification.at(key).get<std::size_t>() > kMaximumPoolSize) {
            return {
                ErrorCode::ConfigInvalid,
                std::string("classification.") + key + " must not exceed 10000"
            };
        }
    }
    const auto threshold = classification
        .at("never_cull_if_data_confidence_below").get<double>();
    if (!std::isfinite(threshold) || threshold < 0.0 || threshold > 1.0) {
        return {
            ErrorCode::ConfigInvalid,
            "classification.never_cull_if_data_confidence_below must be between 0 and 1"
        };
    }

    const auto& planning = value.at("room_planning");
    result = ValidateModuleVersion(planning, "room_planning");
    if (!result) {
        return result;
    }
    const auto capacity = planning.at("default_soft_capacity").get<std::size_t>();
    const auto overflow = planning.at("max_soft_overflow_per_room")
        .get<std::size_t>();
    if (capacity < 1 || capacity > kMaximumRoomPreference) {
        return {
            ErrorCode::ConfigInvalid,
            "room_planning.default_soft_capacity must be between 1 and 1000"
        };
    }
    if (overflow > kMaximumRoomPreference) {
        return {
            ErrorCode::ConfigInvalid,
            "room_planning.max_soft_overflow_per_room must be between 0 and 1000"
        };
    }
    if (!planning.at("never_exceed_known_hard_capacity").get<bool>() ||
        !planning.at("allow_partial_plan").get<bool>()) {
        return {
            ErrorCode::ConfigInvalid,
            "room planning hard-capacity and partial-plan safety rules cannot be disabled"
        };
    }
    (void)planning.at("allow_soft_overflow").get<bool>();
    (void)planning.at("prefer_single_combat_staging_room").get<bool>();
    (void)planning.at("keep_breeding_pairs_together").get<bool>();
    (void)planning.at("avoid_inbreeding_pairs").get<bool>();
    (void)planning.at("keep_kittens_separate_when_possible").get<bool>();
    return {};
}

Result<void> ValidateMarkerAndDiagnostics(const Json& value) {
    const auto& marker = value.at("recommendation_marker");
    auto result = ValidateModuleVersion(marker, "recommendation_marker");
    if (!result) {
        return result;
    }
    const auto count = marker.at("recommended_count").get<std::size_t>();
    const auto pulse = marker.at("pulse_top_n").get<std::size_t>();
    if (count < 1 || count > 100) {
        return {
            ErrorCode::ConfigInvalid,
            "recommendation_marker.recommended_count must be between 1 and 100"
        };
    }
    if (pulse != 0) {
        return {
            ErrorCode::ConfigInvalid,
            "recommendation_marker.pulse_top_n must remain 0 until a verified visual adapter exists"
        };
    }
    if (!marker.at("never_auto_select").get<bool>()) {
        return {
            ErrorCode::ConfigInvalid,
            "recommendation_marker.never_auto_select cannot be disabled"
        };
    }
    (void)marker.at("show_score").get<bool>();
    (void)marker.at("show_rank").get<bool>();
    if (!marker.at("auto_clear_on_scene_exit").get<bool>() ||
        !marker.at("recompute_if_stale").get<bool>()) {
        return {
            ErrorCode::ConfigInvalid,
            "recommendation markers must clear on scene exit and recompute when stale"
        };
    }

    const auto& diagnostics = value.at("diagnostics");
    result = ValidateModuleVersion(diagnostics, "diagnostics");
    if (!result) {
        return result;
    }
    (void)diagnostics.at("show_debug_overlay").get<bool>();
    (void)diagnostics.at("export_scene_summary_enabled").get<bool>();
    return {};
}

}  // namespace

Result<void> ValidateConfigJson(const Json& value) {
    try {
        if (!value.at("schema_version").is_number_integer() ||
            value.at("schema_version").get<int>() < 1) {
            return {ErrorCode::ConfigInvalid, "schema_version must be a positive integer"};
        }
        auto result = ValidateGeneralAndUi(value);
        if (!result) {
            return result;
        }
        result = ValidateSafety(value);
        if (!result) {
            return result;
        }
        result = ValidateScoring(value.at("combat_scoring"), "combat_scoring", true);
        if (!result) {
            return result;
        }
        result = ValidateScoring(
            value.at("breeding_scoring"),
            "breeding_scoring",
            false);
        if (!result) {
            return result;
        }
        result = ValidateProtectionAndPlanning(value);
        if (!result) {
            return result;
        }
        return ValidateMarkerAndDiagnostics(value);
    } catch (const Json::exception& exception) {
        return {ErrorCode::ConfigInvalid, exception.what()};
    }
}

}  // namespace autocattery::config_detail
