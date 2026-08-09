#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

#include "auto_cattery/ui/scene_context.hpp"
#include "mew_ui_furniture_move_probe.h"

namespace autocattery::ui {

enum class FurnitureMoveProbeEventKind {
    None,
    BeforeCaptured,
    ReportWritten,
    Cancelled,
    Rejected
};

struct FurnitureMoveProbeEvent {
    FurnitureMoveProbeEventKind kind{FurnitureMoveProbeEventKind::None};
    std::string message;
    std::string report_filename;
};

class FurnitureMoveProbeController {
public:
    void Initialize(
        const std::filesystem::path& game_executable,
        std::filesystem::path diagnostics_root);
    void Shutdown() noexcept;

    [[nodiscard]] FurnitureMoveProbeEvent Poll(
        const UiContextSnapshot& context,
        void* house_scene_manager,
        bool furniture_mode,
        void* furniture_ui,
        bool capture_pressed);

private:
    void ClearCapture() noexcept;
    [[nodiscard]] static bool SafeFurnitureMode(
        const UiContextSnapshot& context,
        void* house_scene_manager,
        bool furniture_mode,
        void* furniture_ui);

    bool build_supported_{};
    std::uint64_t scene_generation_{};
    void* house_scene_manager_{};
    void* furniture_ui_{};
    std::filesystem::path diagnostics_root_;
    std::unique_ptr<AcMewFurnitureMoveSample> before_furniture_ui_;
    std::unique_ptr<AcMewFurnitureMoveSample> before_house_scene_;
};

}  // namespace autocattery::ui
