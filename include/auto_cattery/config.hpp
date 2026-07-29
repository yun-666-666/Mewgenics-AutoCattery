#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "auto_cattery/breeding/domain.hpp"
#include "auto_cattery/classification/domain.hpp"
#include "auto_cattery/error.hpp"
#include "auto_cattery/room_planning/domain.hpp"
#include "auto_cattery/scoring/domain.hpp"

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

struct ExecutionConfig {
    bool real_write_adapter_enabled{};
    bool cull_enabled{};
    bool require_quiescent_backup{true};
};

struct WorkflowConfig {
    std::uint32_t preview_ttl_seconds{120};
};

struct ProtectionConfig {
    std::uint32_t version{1};
    std::string sidecar_file{"protection.json"};
    bool protect_unknown_native_state{true};
    bool require_stable_identity_for_sidecar{true};
};

struct Config {
    int schema_version{1};
    bool mod_enabled{true};
    bool safe_mode{true};
    std::string log_level{"info"};
    std::string language{"zh-CN"};
    UiConfig ui;
    SafetyConfig safety;
    ExecutionConfig execution;
    WorkflowConfig workflow;
    scoring::CombatScoringConfig combat_scoring;
    breeding::BreedingScoringConfig breeding_scoring;
    classification::ClassificationConfig classification;
    ProtectionConfig protection;
    room_planning::RoomPlanningConfig room_planning;
    bool force_read_only{false};
};

Result<Config> LoadConfig(
    const std::filesystem::path& default_path,
    const std::filesystem::path& user_path);

}  // namespace autocattery
