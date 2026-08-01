#pragma once

#include <mutex>
#include <optional>

#include "auto_cattery/snapshot/game_read_adapter.hpp"
#include "auto_cattery/snapshot/save_snapshot_adapter.hpp"
#include "runtime_house_state.hpp"

namespace autocattery::ui {

class RuntimeMatchedSaveSnapshotAdapter final
    : public snapshot::IGameReadAdapter {
public:
    void SetRuntimeContext(
        std::size_t house_cat_count,
        std::size_t available_room_count) noexcept;
    void SetRuntimeHouseState(RuntimeHouseState state) noexcept;

    Result<snapshot::HouseSnapshot> CaptureHouseSnapshot(
        std::uint64_t scene_generation) override;

private:
    snapshot::SaveSnapshotAdapter saves_;
    std::mutex context_mutex_;
    std::size_t house_cat_count_{};
    std::size_t available_room_count_{};
    std::optional<RuntimeHouseState> runtime_state_;
};

}  // namespace autocattery::ui
