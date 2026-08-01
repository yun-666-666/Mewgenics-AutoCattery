#include "runtime_house_state_capture.hpp"

#include <algorithm>
#include <array>

#include "mew_ui_house_cat_probe.h"
#include "mew_ui_house_move_adapter.h"
#include "mew_ui_house_move_probe.h"
#include "mew_ui_live_house_state.h"

namespace autocattery::ui {

Result<RuntimeHouseState> CaptureRuntimeHouseState(
    void* house_scene_manager) {
    if (!house_scene_manager) {
        return {{}, ErrorCode::SceneUnavailable,
                "House scene is unavailable"};
    }
    const auto cat_count =
        AcMewCountHouseCats(house_scene_manager);
    if (cat_count == 0U) {
        return {{}, ErrorCode::CatDataUnavailable,
                "runtime House cats are unavailable"};
    }
    std::vector<AcMewLiveHouseCatState> cats(cat_count);
    const auto captured = AcMewCaptureLiveHouseCats(
        house_scene_manager,
        cats.data(),
        cats.size());
    if (captured != cat_count) {
        return {{}, ErrorCode::CatDataUnavailable,
                "runtime House cat state capture is incomplete"};
    }

    std::array<void*, 16> native_rooms{};
    const auto native_count = AcMewEnumerateNativeHouseRooms(
        house_scene_manager,
        native_rooms.data(),
        native_rooms.size());
    RuntimeHouseState result;
    result.available_room_count = std::clamp<std::size_t>(
        native_count > 2U ? native_count - 2U : 2U,
        2U,
        4U);
    result.cats.reserve(cats.size());
    for (const auto& cat : cats) {
        result.cats.push_back({
            cat.cat_id,
            reinterpret_cast<RuntimePointer>(cat.component),
            reinterpret_cast<RuntimePointer>(cat.room)
        });
    }
    result.rooms.reserve(native_count);
    for (std::size_t index = 0; index < native_count; ++index) {
        RuntimeRoomEvidence evidence;
        evidence.room =
            reinterpret_cast<RuntimePointer>(native_rooms[index]);
        const auto mask =
            AcMewDetectNativeHouseRoomMask(native_rooms[index]);
        for (std::uint32_t room_index = 0;
             room_index < AC_MEW_MOVE_PROBE_ROOM_COUNT;
             ++room_index) {
            if ((mask & (1U << room_index)) != 0U) {
                const auto* id = AcMewMoveProbeRoomId(room_index);
                if (id) {
                    evidence.detected_ids.emplace_back(id);
                }
            }
        }
        result.rooms.push_back(std::move(evidence));
    }
    return {std::move(result)};
}

}  // namespace autocattery::ui
