#include "auto_cattery/ui/mew_ui_bridge.hpp"

#include "auto_cattery/logger.hpp"
#include "auto_cattery/ui/house_button_controller.hpp"
#include "auto_cattery/ui/recommendation_marker_controller.hpp"
#include "auto_cattery/workflow/organize_workflow_facade.hpp"
#include "mew_ui_house_button_view.hpp"
#include "mew_ui_recommendation_marker_view.hpp"

namespace autocattery::ui {

void MewUiBridge::ApplyRuntimeConfig() {
    if (!config_runtime_) {
        return;
    }

    const auto config = config_runtime_->Current();
    if (organize_workflow_) {
        const auto applied = organize_workflow_->ApplyConfig(config);
        if (!applied) {
            Logger::Instance().Write(
                LogLevel::Error,
                "Config",
                "AC1305",
                "Runtime configuration reached the UI boundary but the idle "
                "organize workflow rejected it: " + applied.message);
            return;
        }
    }

    recommendation_scoring_config_ = config.combat_scoring;
    if (house_button_controller_) house_button_controller_->InvalidatePreview();
    recommendation_marker_config_ = config.recommendation_marker;
    const bool english = config.general.language == "en-US";
    if (house_button_view_) house_button_view_->SetEnglish(english);
    if (recommendation_marker_view_)
        recommendation_marker_view_->SetEnglish(english);
#ifndef _DEBUG
    debug_probe_enabled_ =
        config.ui.show_debug_overlay ||
        config.diagnostics.show_debug_overlay;
#endif
    recommendation_detail_targets_.clear();
    mapping_probe_session_.Clear();
    mapping_snapshot_request_active_ = false;
    mapping_snapshot_retry_deadline_ = {};
    mapping_snapshot_next_attempt_ = {};
    if (recommendation_marker_controller_) {
        recommendation_marker_controller_->Detach();
    }
    Logger::Instance().Write(
        LogLevel::Info,
        "Config",
        "AC1303",
        "Runtime configuration applied; recommendation markers and cached "
        "scoring views require recomputation; breeding_population=" +
            std::to_string(config.room_planning.breeding_room_population) +
            " single_combat_room=" +
            std::to_string(config.room_planning.prefer_single_combat_staging_room));
}

}  // namespace autocattery::ui
