#pragma once

#include <filesystem>
#include <unordered_map>

#include "auto_cattery/workflow/execution_router.hpp"

namespace autocattery::ui {

[[nodiscard]] bool IsRuntimeMovePlanApproved(
    const room_planning::RoomPlan& plan) noexcept;

class RuntimeHouseMoveGateway final
    : public workflow::IApprovedTransactionGateway {
public:
    [[nodiscard]] bool Initialize(
        const std::filesystem::path& game_executable);
    void SetHouseScene(void* house_scene_manager) noexcept;

    execution::ExecutionResult ExecuteApproved(
        const workflow::PreviewBundle& bundle,
        workflow::ExecutionChoice choice) override;

private:
    bool build_supported_{};
    void* house_scene_manager_{};
};

}  // namespace autocattery::ui
