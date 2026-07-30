#pragma once

#include <cstdint>
#include <vector>

#include "house_move_probe_report.hpp"
#include "mew_ui_house_move_probe.h"

namespace autocattery::ui {

class HouseMoveProbeSession {
public:
    void Arm(
        std::uint64_t scene_generation,
        std::vector<AcMewHouseCatMatch> matches);
    void Cancel() noexcept;

    [[nodiscard]] bool Armed() const noexcept;
    [[nodiscard]] bool HasBeforeSample() const noexcept;
    [[nodiscard]] std::size_t MatchCount() const noexcept;

    [[nodiscard]] bool CaptureBefore();
    [[nodiscard]] Result<HouseMoveProbeReport> CaptureAfter();

private:
    std::uint64_t scene_generation_{};
    std::vector<AcMewHouseCatMatch> matches_;
    std::vector<AcMewHouseMoveSample> before_;
};

}  // namespace autocattery::ui
