#pragma once

#include <filesystem>
#include <string>

#include "auto_cattery/error.hpp"

namespace autocattery {

struct UiConfig {
    bool house_button_enabled{true};
    bool embark_button_enabled{true};
    bool show_debug_overlay{false};
};

struct SafetyConfig {
    bool require_preview_before_destructive_actions{true};
    bool create_backup_before_apply{true};
    bool abort_on_unknown_game_build{true};
};

struct RecommendationMarkerConfig {
    bool enabled{true};
    bool show_rank{true};
    bool show_score{true};
    bool pulse{true};
    int pulse_period_ms{900};
    int max_markers{8};
    bool fallback_to_text_prefix{true};
};

struct Config {
    int schema_version{1};
    bool mod_enabled{true};
    bool safe_mode{true};
    std::string log_level{"info"};
    std::string language{"zh-CN"};
    UiConfig ui;
    RecommendationMarkerConfig recommendation_marker;
    SafetyConfig safety;
    bool force_read_only{false};
};

Result<Config> LoadConfig(
    const std::filesystem::path& default_path,
    const std::filesystem::path& user_path);

}  // namespace autocattery
