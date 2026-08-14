#include "auto_cattery/ui/mew_ui_bridge.hpp"

#include "auto_cattery/logger.hpp"
#include "auto_cattery/ui/recommendation_marker_controller.hpp"
#include "auto_cattery/workflow/organize_workflow_facade.hpp"
#include "mew_ui_house_button_view.hpp"
#include "mew_ui_recommendation_marker_view.hpp"
#include "runtime_house_state.hpp"

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

    const bool furniture_placement_changed =
        furniture_placement_config_ != config.furniture_placement;
    recommendation_scoring_config_ = config.combat_scoring;
    recommendation_marker_config_ = config.recommendation_marker;
    furniture_placement_config_ = config.furniture_placement;
    if (furniture_placement_changed) {
        furniture_auto_run_active_ = false;
        ClearFurnitureLayoutPreview();
        furniture_locked_room_ids_.clear();
        furniture_locked_room_signatures_.clear();
        furniture_room_purposes_.clear();
        furniture_layout_move_tabu_.clear();
        furniture_layout_attempted_state_edges_.clear();
        furniture_focus_room_id_.reset();
        Logger::Instance().Write(
            LogLevel::Info,
            "FurnitureAnalysis",
            "AC3930",
            "Furniture placement configuration changed; invalidated the old analysis preview, completed-room locks, focus, purposes, and bounded search history. The next analysis will evaluate all rooms under the new targets.");
    }
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
        "scoring views require recomputation.");
}

}  // namespace autocattery::ui
