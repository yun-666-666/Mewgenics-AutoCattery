#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include "auto_cattery/api_types.hpp"
#include "auto_cattery/ui/scene_context.hpp"

namespace autocattery::ui {

class MewUiBridge final : public Module {
public:
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
    std::chrono::steady_clock::time_point last_tick_time_{};
    std::atomic_bool ready_logged_{false};
};

}  // namespace autocattery::ui
