#include "auto_cattery/config.hpp"

#include <fstream>
#include <set>

#include <nlohmann/json.hpp>

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
    } catch (const Json::exception& error) {
        return {{}, ErrorCode::ConfigInvalid, error.what()};
    }

    result.force_read_only = result.schema_version > kSupportedConfigSchema;
    return {std::move(result), ErrorCode::Ok, {}};
}

}  // namespace autocattery
