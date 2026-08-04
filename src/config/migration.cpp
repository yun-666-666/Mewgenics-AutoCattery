#include "config_json.hpp"

#include "auto_cattery/version.hpp"

namespace autocattery::config_detail {
namespace {

Json MigrateV1ToV2(Json input) {
    input["schema_version"] = 2;
    auto& general = input["general"];
    if (!general.is_object()) {
        general = Json::object();
    }
    general["version"] = general.value("version", 1);
    for (const char* key : {"mod_enabled", "safe_mode", "log_level", "language"}) {
        if (input.contains(key)) {
            general[key] = input[key];
        }
    }
    if (input.contains("safety") && !input.contains("execution_safety")) {
        input["execution_safety"] = input["safety"];
    }
    for (const char* key : {
             "ui",
             "combat_scoring",
             "breeding_scoring",
             "classification",
             "protection",
             "room_planning",
             "recommendation_marker",
             "diagnostics",
             "level_up"}) {
        auto& module = input[key];
        if (!module.is_object()) {
            module = Json::object();
        }
        module["version"] = module.value("version", 1);
    }
    auto& safety = input["execution_safety"];
    if (!safety.is_object()) {
        safety = Json::object();
    }
    safety["version"] = safety.value("version", 1);
    input["safety"] = safety;
    return input;
}

}  // namespace

Result<Json> MigrateConfig(Json input) {
    if (!input.is_object()) {
        return {{}, ErrorCode::ConfigInvalid, "configuration root must be an object"};
    }
    try {
        if (input.contains("schema_version") &&
            !input.at("schema_version").is_number_integer()) {
            return {{}, ErrorCode::ConfigInvalid, "schema_version must be an integer"};
        }
        const auto version = input.value("schema_version", 1);
        if (version == 1) {
            return {MigrateV1ToV2(std::move(input))};
        }
        if (version >= 2) {
            return {std::move(input)};
        }
        return {{}, ErrorCode::ConfigInvalid, "schema_version is not supported"};
    } catch (const Json::exception& exception) {
        return {{}, ErrorCode::ConfigInvalid, exception.what()};
    }
}

void ApplySessionOverride(
    Json& merged,
    const SessionConfigOverride& session_override) {
    static constexpr std::array<const char*, snapshot::kStatCount> stat_keys{
        "strength",
        "dexterity",
        "constitution",
        "intelligence",
        "speed",
        "charisma",
        "luck"
    };
    if (session_override.combat_recommended_count) {
        merged["combat_scoring"]["recommended_count"] =
            *session_override.combat_recommended_count;
        merged["recommendation_marker"]["recommended_count"] =
            *session_override.combat_recommended_count;
    }
    if (session_override.combat_exclude_injured) {
        merged["combat_scoring"]["exclude_injured"] =
            *session_override.combat_exclude_injured;
    }
    const auto apply_weights = [&](
                                   const char* module,
                                   const auto& weights) {
        if (!weights) {
            return;
        }
        for (std::size_t index = 0; index < stat_keys.size(); ++index) {
            merged[module]["stat_weights"][stat_keys[index]] =
                (*weights)[index];
        }
    };
    apply_weights("combat_scoring", session_override.combat_stat_weights);
    apply_weights("breeding_scoring", session_override.breeding_stat_weights);
    if (session_override.minimum_general_reserve) {
        merged["classification"]["minimum_general_reserve"] =
            *session_override.minimum_general_reserve;
    }
    if (session_override.allow_soft_overflow) {
        merged["room_planning"]["allow_soft_overflow"] =
            *session_override.allow_soft_overflow;
    }
    if (session_override.read_only_mode) {
        merged["execution_safety"]["read_only_mode"] =
            *session_override.read_only_mode;
    }
    if (session_override.create_backup_before_apply) {
        merged["execution_safety"]["create_backup_before_apply"] =
            *session_override.create_backup_before_apply;
    }
    if (session_override.single_click_execute) {
        merged["execution_safety"]["single_click_execute"] =
            *session_override.single_click_execute;
    }
}

Result<void> EnforceHardSafety(Json& merged) {
    try {
        const auto& safety = merged.at("execution_safety");
        if (!safety.at("create_backup_before_apply").is_boolean()) {
            return {
                ErrorCode::ConfigInvalid,
                "execution_safety.create_backup_before_apply must be boolean"
            };
        }
        merged["safety"] = safety;
        if (!safety.at("create_backup_before_apply").get<bool>()) {
            merged["execution"]["cull_enabled"] = false;
        }
        return {};
    } catch (const Json::exception& exception) {
        return {ErrorCode::ConfigInvalid, exception.what()};
    }
}

}  // namespace autocattery::config_detail
