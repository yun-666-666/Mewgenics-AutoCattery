#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "auto_cattery/api_types.hpp"
#include "auto_cattery/ui/scene_context.hpp"

namespace autocattery::workflow {
class OrganizeWorkflowFacade;
}

namespace autocattery::ui {

class HouseButtonController;
class MewUiHouseButtonView;

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
    SceneObservation ObserveScenes(
        const std::vector<RuntimeScene>& scenes) const;
    void LogSceneSummary(const std::vector<RuntimeScene>& scenes);
    void ExportSceneSummary(const std::vector<RuntimeScene>& scenes) const;

    bool started_{};
    bool debug_probe_enabled_{};
    std::filesystem::path diagnostics_root_;
    SceneSignatures signatures_;
    SceneContextService scene_context_;
    std::uint64_t scene_subscription_{};
    std::string last_scene_summary_;
    std::string last_house_attach_error_;
    std::chrono::steady_clock::time_point last_tick_time_{};
    std::chrono::steady_clock::time_point next_house_attach_retry_{};
    std::atomic_bool ready_logged_{false};
    std::unique_ptr<MewUiHouseButtonView> house_button_view_;
    std::unique_ptr<workflow::OrganizeWorkflowFacade> organize_workflow_;
    std::unique_ptr<HouseButtonController> house_button_controller_;
};

}  // namespace autocattery::ui
