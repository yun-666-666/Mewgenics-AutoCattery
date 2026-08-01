#include "runtime_house_move_gateway.hpp"

#include <unordered_map>

#include "auto_cattery/logger.hpp"
#include "auto_cattery/save_safety/game_build_gate.hpp"
#include "mew_ui_house_move_adapter.h"
#include "runtime_house_state.hpp"
#include "runtime_house_state_capture.hpp"

namespace autocattery::ui {

bool IsRuntimeMovePlanApproved(
    const room_planning::RoomPlan& plan) noexcept {
    return plan.move_execution_allowed ||
        (plan.moves.empty() && plan.fully_satisfied &&
         plan.disposition == room_planning::PlanDisposition::Complete &&
         plan.validation_errors.empty());
}

bool RuntimeHouseMoveGateway::Initialize(
    const std::filesystem::path& game_executable) {
    build_supported_ = static_cast<bool>(
        save_safety::CurrentMewgenicsBuildGate().Verify(game_executable));
    return build_supported_;
}

void RuntimeHouseMoveGateway::SetHouseScene(
    void* house_scene_manager) noexcept {
    house_scene_manager_ = house_scene_manager;
}

execution::ExecutionResult RuntimeHouseMoveGateway::ExecuteApproved(
    const workflow::PreviewBundle& bundle,
    workflow::ExecutionChoice choice) {
    execution::ExecutionResult result;
    if (!build_supported_ || !house_scene_manager_ ||
        choice != workflow::ExecutionChoice::MoveOnly ||
        !IsRuntimeMovePlanApproved(bundle.room_plan)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "RuntimeHouseMove",
            "AC14300",
            "Move execution rejected before runtime capture.");
        result.failure_reason = execution::FailureReason::Unsupported;
        return result;
    }

    const auto runtime =
        CaptureRuntimeHouseState(house_scene_manager_);
    if (!runtime) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "RuntimeHouseMove",
            "AC14301",
            "Runtime House state capture failed: " + runtime.message);
        result.failure_reason =
            execution::FailureReason::PreconditionsChanged;
        return result;
    }
    if (!RuntimeHouseStateMatches(bundle.snapshot, runtime.value)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "RuntimeHouseMove",
            "AC14316",
            "Room assignments changed after preview; no moves were made.");
        result.failure_reason =
            execution::FailureReason::PreconditionsChanged;
        return result;
    }
    if (bundle.room_plan.moves.empty()) {
        result.committed = true;
        Logger::Instance().Write(
            LogLevel::Info,
            "RuntimeHouseMove",
            "AC14304",
            "Native House moves committed=0");
        return result;
    }
    const auto rooms =
        ResolveRuntimeRoomPointers(bundle.snapshot, runtime.value);
    if (!rooms) {
        result.failure_reason =
            execution::FailureReason::PreconditionsChanged;
        return result;
    }
    std::unordered_map<snapshot::CatId, RuntimeCatRoomState> cats;
    for (const auto& cat : runtime.value.cats) {
        cats.emplace(cat.cat_id, cat);
    }

    for (const auto& move : bundle.room_plan.moves) {
        if (!move.executable) {
            continue;
        }
        const auto cat = cats.find(move.cat_id);
        const auto target = rooms.value.find(move.to_room);
        if (cat == cats.end() || target == rooms.value.end()) {
            result.failure_reason =
                execution::FailureReason::PreconditionsChanged;
            return result;
        }
        void* component = reinterpret_cast<void*>(cat->second.component);
        void* target_room = reinterpret_cast<void*>(target->second);
        if (AcMewReadHouseCatCurrentRoom(component) == target_room) {
            continue;
        }
        const auto moved =
            AcMewInvokeNativeHouseMove(component, target_room);
        if (!moved.invoked || !moved.committed) {
            Logger::Instance().Write(
                LogLevel::Error,
                "RuntimeHouseMove",
                "AC14303",
                "Native House move failed: signature=" +
                    std::to_string(moved.signature_valid) +
                    " cat=" + std::to_string(moved.cat_valid) +
                    " target=" +
                    std::to_string(moved.target_room_valid) +
                    " invoked=" + std::to_string(moved.invoked) +
                    " committed=" + std::to_string(moved.committed) +
                    " exception=" + std::to_string(moved.seh_code));
            result.failure_reason = execution::FailureReason::MoveFailed;
            return result;
        }
        ++result.completed_moves;
    }
    result.committed = true;
    Logger::Instance().Write(
        LogLevel::Info,
        "RuntimeHouseMove",
        "AC14304",
        "Native House moves committed=" +
            std::to_string(result.completed_moves));
    return result;
}

}  // namespace autocattery::ui
