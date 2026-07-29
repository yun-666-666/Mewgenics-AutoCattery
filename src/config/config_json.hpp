#pragma once

#include <filesystem>

#include <nlohmann/json.hpp>

#include "auto_cattery/config.hpp"

namespace autocattery::config_detail {

using Json = nlohmann::json;

Json SafeDefaultsJson();
Result<Json> ReadJsonIfPresent(const std::filesystem::path& path);
Result<Json> MigrateConfig(Json input);
void ApplySessionOverride(
    Json& merged,
    const SessionConfigOverride& session_override);
Result<void> EnforceHardSafety(Json& merged);
Result<void> ValidateConfigJson(const Json& value);
Result<Config> DecodeConfig(const Json& value);

}  // namespace autocattery::config_detail
