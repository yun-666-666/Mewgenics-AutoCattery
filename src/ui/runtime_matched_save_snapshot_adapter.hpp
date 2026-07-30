#pragma once

#include <atomic>

#include "auto_cattery/snapshot/game_read_adapter.hpp"
#include "auto_cattery/snapshot/save_snapshot_adapter.hpp"

namespace autocattery::ui {

class RuntimeMatchedSaveSnapshotAdapter final
    : public snapshot::IGameReadAdapter {
public:
    void SetRuntimeContext(
        std::size_t house_cat_count,
        std::size_t available_room_count) noexcept;

    Result<snapshot::HouseSnapshot> CaptureHouseSnapshot(
        std::uint64_t scene_generation) override;

private:
    snapshot::SaveSnapshotAdapter saves_;
    std::atomic_size_t house_cat_count_{};
    std::atomic_size_t available_room_count_{};
};

}  // namespace autocattery::ui
