#include "in_game_panel_controller.hpp"

#include <algorithm>

#include "auto_cattery/logger.hpp"
#include "mew_ui_house_cat_probe.h"

namespace autocattery::ui {
namespace {
bool PopulationWritesEnabled(const Config& config) {
    return config.mod_enabled && !config.safe_mode && !config.force_read_only &&
        !config.execution_safety.read_only_mode;
}
}  // namespace

void InGamePanelController::CancelPopulationAutomation() noexcept {
    population_auto_loading_ = false;
    population_auto_preview_ = false;
    population_countdown_ = -1;
}

void InGamePanelController::PollPopulationAutomation(const UiContextSnapshot& context) {
    const auto now = std::chrono::steady_clock::now();
    if (population_auto_preview_) {
        const auto seconds = std::max(0, static_cast<int>(
            std::chrono::ceil<std::chrono::seconds>(population_deadline_ - now).count()));
        if (seconds == population_countdown_) return;
        population_countdown_ = seconds;
        const auto loaded = settings_.Reload();
        if (!loaded || !PopulationWritesEnabled(settings_.CurrentConfig()) ||
            settings_.CurrentConfig().room_planning.population_limit != population_checked_limit_ ||
            !workflow::CanReloadConfig(workflow_.State())) {
            CancelPopulationAutomation();
            population_error_ = "设置或整理状态已变化，本轮自动淘汰已取消";
            Render();
            return;
        }
        if (seconds > 0) {
            Render();
            return;
        }
        CancelPopulationAutomation();
        StartDeadCatDelivery();
        if (open_) Render();
        return;
    }

    if (population_auto_loading_) {
        PollProtectionLoad();
        if (protection_loading_) return;
        population_auto_loading_ = false;
        if (!settings_.Reload() || !PopulationWritesEnabled(settings_.CurrentConfig()) ||
            !workflow::CanReloadConfig(workflow_.State())) return;
        if (!current_save_ || !protection_) {
            Logger::Instance().Write(LogLevel::Warn, "PopulationAutomation", "AC4201",
                "Automatic population check stopped: " + status_);
            return;
        }
        population_checked_limit_ = settings_.CurrentConfig().room_planning.population_limit;
        const auto& save = protection_->saves()[*current_save_];
        population_plan_ = breeding::SelectPopulation(
            save.snapshot, protection_->cats(), settings_.CurrentConfig());
        if (!*population_plan_ || population_plan_->value.surplus.empty()) {
            Logger::Instance().Write(LogLevel::Info, "PopulationAutomation", "AC4201",
                !*population_plan_ ? population_plan_->message :
                    "No unprotected surplus cats; retained=" +
                        std::to_string(population_plan_->value.retained.size()));
            return;
        }
        page_ = ManagementPanelPage::Senior;
        population_preview_ = true;
        delivery_preview_ = false;
        senior_page_ = 0;
        open_ = true;
        population_auto_preview_ = true;
        population_deadline_ = now + std::chrono::seconds(10);
        population_countdown_ = 10;
        Logger::Instance().Write(LogLevel::Info, "PopulationAutomation", "AC4200",
            "Automatic surplus preview; cats=" + std::to_string(save.snapshot.cats.size()) +
            " limit=" + std::to_string(population_checked_limit_) +
            " surplus=" + std::to_string(population_plan_->value.surplus.size()) +
            "; delivery in 10 seconds; Esc/F10/Close cancels this house visit.");
        Render();
        return;
    }

    if (open_ || protection_loading_ || delivery_.Active() || !house_scene_manager_ ||
        !workflow::CanReloadConfig(workflow_.State()) || now < population_next_check_) return;
    population_next_check_ = now + std::chrono::seconds(2);
    if (!settings_.Reload() || !PopulationWritesEnabled(settings_.CurrentConfig())) return;
    const auto limit = settings_.CurrentConfig().room_planning.population_limit;
    if (population_checked_generation_ == context.scene_generation &&
        population_checked_limit_ == limit) return;
    population_checked_generation_ = context.scene_generation;
    population_checked_limit_ = limit;
    if (AcMewCountHouseCats(house_scene_manager_) <= limit) return;
    StartProtectionLoad();
    population_auto_loading_ = true;
}
}  // namespace autocattery::ui
