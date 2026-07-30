#pragma once

#include <cstdint>
#include <future>
#include <vector>

#include "auto_cattery/error.hpp"
#include "auto_cattery/snapshot/domain.hpp"
#include "mew_ui_house_cat_probe.h"

namespace autocattery::ui {

enum class HouseMoveMappingStatus {
    Idle,
    Loading,
    Ready,
    Rejected
};

struct HouseMoveMappingResult {
    HouseMoveMappingStatus status{HouseMoveMappingStatus::Idle};
    std::vector<AcMewHouseCatMatch> matches;
    std::string message;
};

class HouseMoveProbeMapper {
public:
    // Starts a read-only disk snapshot task for one House generation.
    void Start(std::uint64_t scene_generation);
    void Cancel() noexcept;

    [[nodiscard]] HouseMoveMappingResult Poll(
        std::uint64_t scene_generation,
        void* house_scene_manager);
    [[nodiscard]] bool Loading() const noexcept;

private:
    std::uint64_t requested_generation_{};
    bool canceled_{};
    std::future<Result<std::vector<snapshot::HouseSnapshot>>> task_;
};

}  // namespace autocattery::ui
