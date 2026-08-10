#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "auto_cattery/api_types.hpp"
#include "auto_cattery/config_runtime.hpp"
#include "auto_cattery/furniture_analysis/domain.hpp"
#include "auto_cattery/recommendation/mapping_probe.hpp"
#include "auto_cattery/scoring/domain.hpp"
#include "auto_cattery/snapshot/domain.hpp"
#include "auto_cattery/ui/scene_context.hpp"

namespace autocattery::workflow {
class OrganizeWorkflowFacade;
}

namespace autocattery::furniture_analysis {
class FurnitureAnalysisService;
}

namespace autocattery::ui {

class HouseButtonController;
class HouseMoveProbeController;
class FurniturePlacementGateway;
class FurnitureMoveProbeController;
class InGamePanelController;
class MewUiHouseButtonView;
class MewUiManagementPanelView;
class RecommendationMarkerController;
class MewUiRecommendationMarkerView;
class RuntimeHouseMoveGateway;
class RuntimeMatchedSaveSnapshotAdapter;

class MewUiBridge final : public Module {
public:
    MewUiBridge();
    ~MewUiBridge() override;
    [[nodiscard]] const char* Name() const noexcept override;
    bool Initialize(const InitContext& context) override;
    void Shutdown() noexcept override;
    [[nodiscard]] bool Available() const noexcept;
    [[nodiscard]] bool Ready() const noexcept;
    [[nodiscard]] SceneContextService& SceneContext() noexcept;

private:
    struct RuntimeScene;

    static void __cdecl Tick(void* user_data);
    void OnTick();
    void UpdateHouseUiMode(
        const UiContextSnapshot& context,
        const std::vector<RuntimeScene>& scenes);
#ifdef _DEBUG
    void RunFurnitureNativeMoveTest(
        const UiContextSnapshot& context);
#endif
    void ClearFurnitureLayoutPreview();
    void StartFurnitureAutoPlacement(std::uint64_t generation);
    void PollFurnitureAutoPlacement(
        const UiContextSnapshot& context);
    void RefreshRuntimeSnapshotContext();
    void ApplyRuntimeConfig();
    SceneObservation ObserveScenes(
        const std::vector<RuntimeScene>& scenes) const;
    void ObserveMappingProbe(
        const UiContextSnapshot& context,
        const std::vector<RuntimeScene>& scenes);
    void ObserveHouseCatIdentity(
        const UiContextSnapshot& context,
        const std::vector<RuntimeScene>& scenes);
    void StartMappingSnapshotAttempt();
    void LogSceneSummary(const std::vector<RuntimeScene>& scenes);
    void ExportSceneSummary(const std::vector<RuntimeScene>& scenes) const;

    bool started_{};
    bool debug_probe_enabled_{};
    bool runtime_move_available_{};
    bool furniture_mode_{};
    void* furniture_mode_scene_manager_{};
    std::uint32_t furniture_mode_component_count_{};
    void* furniture_mode_component_{};
    std::filesystem::path diagnostics_root_;
    std::filesystem::path recommendation_sidecar_path_;
    SceneSignatures signatures_;
    SceneContextService scene_context_;
    std::uint64_t scene_subscription_{};
    std::string last_scene_summary_;
    std::string last_house_attach_error_;
    std::string last_recommendation_attach_error_;
    std::chrono::steady_clock::time_point last_tick_time_{};
    std::chrono::steady_clock::time_point next_house_attach_retry_{};
    std::chrono::steady_clock::time_point
        next_recommendation_attach_retry_{};
    std::atomic_bool ready_logged_{false};
    recommendation::MappingProbeSession mapping_probe_session_;
    bool mapping_probe_logged_{};
    bool mapping_identity_logged_{};
    std::uint32_t mapping_probe_request_sequence_{};
    std::uint32_t mapping_snapshot_request_sequence_{};
    std::uint32_t mapping_snapshot_task_sequence_{};
    std::uint32_t mapping_snapshot_attempt_{};
    std::uint64_t mapping_snapshot_generation_{};
    std::uint64_t mapping_snapshot_task_generation_{};
    bool mapping_snapshot_request_active_{};
    std::chrono::steady_clock::time_point
        mapping_snapshot_retry_deadline_{};
    std::chrono::steady_clock::time_point
        mapping_snapshot_next_attempt_{};
    scoring::CombatScoringConfig recommendation_scoring_config_;
    RecommendationMarkerConfig recommendation_marker_config_;
    std::vector<void*> recommendation_detail_targets_;
    std::unique_ptr<RuntimeConfigService> config_runtime_;
    std::future<Result<std::vector<snapshot::HouseSnapshot>>>
        mapping_snapshot_task_;
    std::future<Result<furniture_analysis::FurnitureAnalysisSnapshot>>
        furniture_analysis_task_;
    std::uint64_t furniture_analysis_task_generation_{};
    std::optional<furniture_analysis::FurnitureAnalysisSnapshot>
        furniture_analysis_preview_;
    bool furniture_execution_active_{};
    std::uint64_t furniture_execution_generation_{};
    std::size_t furniture_execution_index_{};
    std::size_t furniture_execution_moved_{};
    std::vector<std::size_t> furniture_execution_committed_move_indices_;
    std::uint64_t furniture_layout_session_generation_{};
    std::vector<snapshot::RoomId> furniture_locked_room_ids_;
    std::unique_ptr<MewUiHouseButtonView> house_button_view_;
    std::unique_ptr<workflow::OrganizeWorkflowFacade> organize_workflow_;
    std::unique_ptr<RuntimeHouseMoveGateway> runtime_move_gateway_;
    std::unique_ptr<FurniturePlacementGateway>
        furniture_placement_gateway_;
    RuntimeMatchedSaveSnapshotAdapter* runtime_snapshot_adapter_{};
    std::unique_ptr<furniture_analysis::FurnitureAnalysisService>
        furniture_analysis_service_;
    std::uint64_t runtime_snapshot_context_generation_{};
    void* current_house_scene_manager_{};
    std::unique_ptr<HouseButtonController> house_button_controller_;
    std::unique_ptr<HouseMoveProbeController> house_move_probe_controller_;
    std::unique_ptr<FurnitureMoveProbeController>
        furniture_move_probe_controller_;
    std::unique_ptr<MewUiManagementPanelView> management_panel_view_;
    std::unique_ptr<InGamePanelController> in_game_panel_controller_;
    std::unique_ptr<MewUiRecommendationMarkerView>
        recommendation_marker_view_;
    std::unique_ptr<RecommendationMarkerController>
        recommendation_marker_controller_;
};

}  // namespace autocattery::ui
