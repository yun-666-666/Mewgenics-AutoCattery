#include "auto_cattery/config.hpp"

#include <utility>

#include "config/config_json.hpp"

namespace autocattery {
namespace {

using config_detail::Json;

Result<void> MergeLayer(
    Json& destination,
    const std::filesystem::path& path,
    const char* layer_name) {
    const auto read = config_detail::ReadJsonIfPresent(path);
    if (!read) {
        return {read.code, std::string(layer_name) + ": " + read.message};
    }
    const auto migrated = config_detail::MigrateConfig(std::move(read.value));
    if (!migrated) {
        return {
            migrated.code,
            std::string(layer_name) + ": " + migrated.message
        };
    }
    destination.merge_patch(migrated.value);
    return {};
}

}  // namespace

Result<Config> LoadConfig(
    const std::filesystem::path& default_path,
    const std::filesystem::path& user_path,
    const SessionConfigOverride& session_override) {
    auto merged = config_detail::SafeDefaultsJson();

    const auto defaults =
        MergeLayer(merged, default_path, "default_config.json");
    if (!defaults) {
        return {{}, defaults.code, defaults.message};
    }
    const auto user = MergeLayer(merged, user_path, "user_config.json");
    if (!user) {
        return {{}, user.code, user.message};
    }

    config_detail::ApplySessionOverride(merged, session_override);
    const auto safety = config_detail::EnforceHardSafety(merged);
    if (!safety) {
        return {{}, safety.code, safety.message};
    }
    const auto validation = config_detail::ValidateConfigJson(merged);
    if (!validation) {
        return {{}, validation.code, validation.message};
    }
    return config_detail::DecodeConfig(merged);
}

Result<Config> LoadConfig(
    const std::filesystem::path& default_path,
    const std::filesystem::path& user_path) {
    return LoadConfig(default_path, user_path, {});
}

}  // namespace autocattery
