#include "runtime_house_move_gateway.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <sstream>
#include <unordered_set>
#include <unordered_map>
#include <vector>

#include "auto_cattery/logger.hpp"
#include "auto_cattery/save_safety/game_build_gate.hpp"
#include "mew_ui_house_cat_probe.h"
#include "mew_ui_house_move_adapter.h"
#include "mew_ui_house_move_probe.h"

namespace autocattery::ui {
namespace {

using CatMatchById =
    std::unordered_map<snapshot::CatId, AcMewHouseCatMatch>;
using RoomPointerById =
    std::unordered_map<snapshot::RoomId, void*>;

bool BuildRuntimeBindings(
    const workflow::PreviewBundle& bundle,
    void* scene,
    CatMatchById& cats,
    RoomPointerById& rooms) {
    std::vector<std::int64_t> cat_ids;
    cat_ids.reserve(bundle.snapshot.cats.size());
    std::unordered_map<
        snapshot::RoomId,
        std::unordered_map<void*, std::size_t>> room_votes;
    for (const auto& cat : bundle.snapshot.cats) {
        cat_ids.push_back(cat.id);
    }
    std::vector<AcMewHouseCatMatch> matches(cat_ids.size());
    const auto identity = AcMewProbeHouseCatIdentity(
        scene,
        cat_ids.data(),
        cat_ids.size(),
        matches.data(),
        matches.size());
    if (identity.stable_bijection == 0 ||
        identity.consistent_mapping == 0 ||
        identity.match_count != cat_ids.size()) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "RuntimeHouseMove",
            "AC14313",
            "HouseCat identity binding failed: runtime=" +
                std::to_string(identity.house_cat_count) +
                " requested=" +
                std::to_string(identity.requested_cat_count) +
                " layouts=" +
                std::to_string(identity.valid_layout_count) +
                " offset=" +
                std::to_string(identity.first_identity_offset) +
                " matched=" +
                std::to_string(identity.match_count) +
                " stable=" +
                std::to_string(identity.stable_bijection) +
                " consistent=" +
                std::to_string(identity.consistent_mapping));
        return false;
    }

    for (const auto& match : matches) {
        cats.emplace(match.cat_id, match);
    }
    for (const auto& cat : bundle.snapshot.cats) {
        if (!cat.room_id) {
            continue;
        }
        const auto runtime_cat = cats.find(cat.id);
        if (runtime_cat == cats.end()) {
            return false;
        }
        void* room = AcMewReadHouseCatCurrentRoom(
            runtime_cat->second.component);
        if (!room) {
            return false;
        }
        ++room_votes[*cat.room_id][room];
    }
    for (const auto& [room_id, votes] : room_votes) {
        const auto best = std::ranges::max_element(
            votes,
            [](const auto& left, const auto& right) {
                return left.second < right.second;
            });
        if (best != votes.end()) {
            rooms.emplace(room_id, best->first);
        }
    }
    std::array<void*, 16> native_rooms{};
    const auto native_room_count =
        AcMewEnumerateNativeHouseRooms(
            scene, native_rooms.data(), native_rooms.size());
    Logger::Instance().Write(
        LogLevel::Info,
        "RoomDetection",
        "AC14311",
        "Native House room components=" +
            std::to_string(native_room_count) +
            ", snapshot room IDs=" +
            std::to_string(bundle.snapshot.rooms.size()));
    for (std::size_t index = 0; index < native_room_count; ++index) {
        const auto mask =
            AcMewDetectNativeHouseRoomMask(native_rooms[index]);
        std::ostringstream detected;
        for (std::uint32_t room_index = 0;
             room_index < AC_MEW_MOVE_PROBE_ROOM_COUNT;
             ++room_index) {
            if ((mask & (1U << room_index)) == 0U) {
                continue;
            }
            if (detected.tellp() > 0) {
                detected << '|';
            }
            detected << AcMewMoveProbeRoomId(room_index);
        }
        Logger::Instance().Write(
            LogLevel::Info,
            "RoomDetection",
            "AC14312",
            "Native room component #" +
                std::to_string(index + 1U) +
                " detected IDs=" +
                (mask == 0U ? "unknown" : detected.str()));
        if (std::has_single_bit(mask)) {
            const auto room_index =
                static_cast<std::uint32_t>(std::countr_zero(mask));
            const auto* detected_id =
                AcMewMoveProbeRoomId(room_index);
            if (detected_id &&
                std::ranges::any_of(
                    bundle.snapshot.rooms,
                    [detected_id](const auto& room) {
                        return room.id == detected_id;
                    })) {
                rooms.insert_or_assign(
                    detected_id,
                    native_rooms[index]);
            }
        }
    }
    std::unordered_set<void*> known_pointers;
    for (const auto& [room_id, pointer] : rooms) {
        (void)room_id;
        known_pointers.insert(pointer);
    }
    std::vector<void*> unknown_pointers;
    for (std::size_t index = 0; index < native_room_count; ++index) {
        if (!known_pointers.contains(native_rooms[index])) {
            unknown_pointers.push_back(native_rooms[index]);
        }
    }
    std::vector<snapshot::RoomId> missing_room_ids;
    for (const auto& room : bundle.snapshot.rooms) {
        if (!rooms.contains(room.id)) {
            missing_room_ids.push_back(room.id);
        }
    }
    if (missing_room_ids.size() == 1U &&
        unknown_pointers.size() == 1U) {
        rooms.emplace(
            std::move(missing_room_ids.front()),
            unknown_pointers.front());
    }
    return !rooms.empty();
}

}  // namespace

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
        !bundle.room_plan.move_execution_allowed) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "RuntimeHouseMove",
            "AC14300",
            "Move execution rejected before runtime binding.");
        result.failure_reason = execution::FailureReason::Unsupported;
        return result;
    }

    CatMatchById cats;
    RoomPointerById rooms;
    if (!BuildRuntimeBindings(
            bundle, house_scene_manager_, cats, rooms)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "RuntimeHouseMove",
            "AC14301",
            "Runtime HouseCat/room binding did not match the preview.");
        result.failure_reason =
            execution::FailureReason::PreconditionsChanged;
        return result;
    }

    for (const auto& move : bundle.room_plan.moves) {
        if (!move.executable) {
            continue;
        }
        const auto cat = cats.find(move.cat_id);
        const auto to_room = rooms.find(move.to_room);
        if (cat == cats.end() ||
            to_room == rooms.end()) {
            result.failure_reason =
                execution::FailureReason::PreconditionsChanged;
            Logger::Instance().Write(
                LogLevel::Warn,
                "RuntimeHouseMove",
                "AC14302",
                "A planned cat or target room could not be bound: cat=" +
                    std::to_string(move.cat_id) +
                    " target=" + move.to_room +
                    " completed=" +
                    std::to_string(result.completed_moves));
            return result;
        }
        const auto current_room =
            AcMewReadHouseCatCurrentRoom(cat->second.component);
        if (!current_room) {
            result.failure_reason =
                execution::FailureReason::PreconditionsChanged;
            Logger::Instance().Write(
                LogLevel::Warn,
                "RuntimeHouseMove",
                "AC14302",
                "A planned cat has no readable current room: cat=" +
                    std::to_string(move.cat_id) +
                    " completed=" +
                    std::to_string(result.completed_moves));
            return result;
        }
        if (current_room == to_room->second) {
            continue;
        }
        const auto moved = AcMewInvokeNativeHouseMove(
            cat->second.component,
            to_room->second);
        if (!moved.invoked || !moved.committed) {
            Logger::Instance().Write(
                LogLevel::Error,
                "RuntimeHouseMove",
                "AC14303",
                "Native House move failed: signature=" +
                    std::to_string(moved.signature_valid) +
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
