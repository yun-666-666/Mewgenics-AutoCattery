#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

#include "auto_cattery/breeding/domain.hpp"
#include "auto_cattery/classification/domain.hpp"
#include "auto_cattery/error.hpp"
#include "auto_cattery/room_planning/domain.hpp"
#include "auto_cattery/scoring/domain.hpp"

namespace autocattery {

struct GeneralConfig {
    std::uint32_t version{1};
    bool mod_enabled{true};
    bool safe_mode{true};
    std::string log_level{"info"};
    std::string language{"zh-CN"};
};

struct UiConfig {
    std::uint32_t version{1};
    bool house_button_enabled{true};
    bool embark_button_enabled{true};
    bool show_debug_overlay{false};
};

struct ExecutionSafetyConfig {
    std::uint32_t version{1};
    bool require_preview_before_destructive_actions{true};
    bool create_backup_before_apply{true};
    bool read_only_mode{true};
    bool single_click_execute{false};
    bool abort_on_unknown_game_build{true};
};

using SafetyConfig = ExecutionSafetyConfig;

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

struct RecommendationMarkerConfig {
    std::uint32_t version{1};
    std::size_t recommended_count{8};
    bool show_score{true};
    bool show_rank{true};
    std::size_t pulse_top_n{};
    bool auto_clear_on_scene_exit{true};
    bool recompute_if_stale{true};
    bool never_auto_select{true};
};

struct DiagnosticsConfig {
    std::uint32_t version{1};
    bool show_debug_overlay{false};
    bool export_scene_summary_enabled{false};
    bool collect_cat_data{false};
};

struct LevelUpConfig {
    std::uint32_t version{1};
    std::size_t reroll_count{3};
};

struct Config {
    int schema_version{2};
    GeneralConfig general;
    bool mod_enabled{true};
    bool safe_mode{true};
    std::string log_level{"info"};
    std::string language{"zh-CN"};
    UiConfig ui;
    SafetyConfig safety;
    ExecutionSafetyConfig execution_safety;
    ExecutionConfig execution;
    WorkflowConfig workflow;
    scoring::CombatScoringConfig combat_scoring;
    breeding::BreedingScoringConfig breeding_scoring;
    classification::ClassificationConfig classification;
    ProtectionConfig protection;
    room_planning::RoomPlanningConfig room_planning;
    RecommendationMarkerConfig recommendation_marker;
    DiagnosticsConfig diagnostics;
    LevelUpConfig level_up;
    bool force_read_only{false};
};

struct SessionConfigOverride {
    std::optional<std::size_t> combat_recommended_count;
    std::optional<bool> combat_exclude_injured;
    std::optional<std::array<double, snapshot::kStatCount>> combat_stat_weights;
    std::optional<std::array<double, snapshot::kStatCount>> breeding_stat_weights;
    std::optional<std::size_t> minimum_general_reserve;
    std::optional<bool> allow_soft_overflow;
    std::optional<bool> read_only_mode;
    std::optional<bool> create_backup_before_apply;
    std::optional<bool> single_click_execute;
};

Result<Config> LoadConfig(
    const std::filesystem::path& default_path,
    const std::filesystem::path& user_path);

Result<Config> LoadConfig(
    const std::filesystem::path& default_path,
    const std::filesystem::path& user_path,
    const SessionConfigOverride& session_override);

}  // namespace autocattery
