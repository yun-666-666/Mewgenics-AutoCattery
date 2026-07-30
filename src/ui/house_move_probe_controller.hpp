#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "house_move_probe_mapper.hpp"
#include "house_move_probe_session.hpp"
#include "auto_cattery/ui/scene_context.hpp"

namespace autocattery::ui {

enum class HouseMoveProbeEventKind {
    None,
    Loading,
    Ready,
    Disabled,
    BeforeCaptured,
    ReportWritten,
    Rejected
};

struct HouseMoveProbeEvent {
    HouseMoveProbeEventKind kind{HouseMoveProbeEventKind::None};
    std::string message;
};

class HouseMoveProbeController {
public:
    void Initialize(
        const std::filesystem::path& game_executable,
        std::filesystem::path diagnostics_root);
    void Shutdown() noexcept;

    [[nodiscard]] HouseMoveProbeEvent Poll(
        const UiContextSnapshot& context,
        void* house_scene_manager,
        bool toggle_pressed,
        bool left_mouse_down);

private:
    [[nodiscard]] static bool SafeHouse(
        const UiContextSnapshot& context,
        void* house_scene_manager);
    [[nodiscard]] HouseMoveProbeEvent Disable(std::string message);

    bool build_supported_{};
    bool enabled_{};
    bool previous_left_down_{};
    std::uint64_t scene_generation_{};
    std::filesystem::path diagnostics_root_;
    HouseMoveProbeMapper mapper_;
    HouseMoveProbeSession session_;
};

}  // namespace autocattery::ui
