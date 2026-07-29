#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "auto_cattery/config_runtime.hpp"

namespace autocattery {

enum class SettingsPageKind {
    Simple,
    Advanced,
    Safety
};

struct SettingsControl {
    std::string key;
    std::string label;
    std::string value;
    bool destructive_related{};
    bool read_only{};
};

struct SettingsPage {
    SettingsPageKind kind{SettingsPageKind::Simple};
    std::string title;
    std::vector<SettingsControl> controls;
};

struct SimpleSettingsUpdate {
    std::optional<std::size_t> recommended_count;
    std::optional<bool> exclude_injured;
    std::optional<std::size_t> minimum_general_reserve;
    std::optional<bool> allow_soft_overflow;
};

struct AdvancedSettingsUpdate {
    std::optional<std::array<double, snapshot::kStatCount>> combat_stat_weights;
    std::optional<std::array<double, snapshot::kStatCount>> breeding_stat_weights;
};

struct SafetySettingsUpdate {
    std::optional<bool> read_only_mode;
    std::optional<bool> single_click_execute;
    std::optional<bool> create_backup_before_apply;
    bool confirm_single_click_enable{};
};

class SettingsService final {
public:
    explicit SettingsService(RuntimeConfigService& runtime);

    [[nodiscard]] Config Current() const;
    [[nodiscard]] std::vector<SettingsPage> Pages() const;
    [[nodiscard]] Result<void> ApplySimple(
        const SimpleSettingsUpdate& update,
        workflow::WorkflowState state);
    [[nodiscard]] Result<void> ApplyAdvanced(
        const AdvancedSettingsUpdate& update,
        workflow::WorkflowState state);
    [[nodiscard]] Result<void> ApplySafety(
        const SafetySettingsUpdate& update,
        workflow::WorkflowState state);
    [[nodiscard]] Result<void> AdjustControl(
        std::string_view key,
        int direction,
        bool toggle,
        bool confirm_single_click_enable,
        workflow::WorkflowState state);

private:
    RuntimeConfigService& runtime_;
};

}  // namespace autocattery
