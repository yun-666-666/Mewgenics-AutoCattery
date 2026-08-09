#pragma once

#include <filesystem>
#include <mutex>
#include <optional>

#include "auto_cattery/furniture_analysis/domain.hpp"
#include "auto_cattery/snapshot/game_read_adapter.hpp"
#include "auto_cattery/snapshot/save_snapshot_adapter.hpp"
#include "runtime_house_state.hpp"

namespace autocattery::ui {

class RuntimeMatchedSaveSnapshotAdapter final
    : public snapshot::IGameReadAdapter,
      public furniture_analysis::IFurnitureAnalysisSource {
public:
    explicit RuntimeMatchedSaveSnapshotAdapter(
        std::filesystem::path game_root = {});

    void SetRuntimeContext(
        std::size_t house_cat_count,
        std::size_t available_room_count) noexcept;
    void SetRuntimeHouseState(RuntimeHouseState state) noexcept;

    Result<snapshot::HouseSnapshot> CaptureHouseSnapshot(
        std::uint64_t scene_generation) override;
    Result<furniture_analysis::FurnitureAnalysisSourceSnapshot> Capture(
        std::uint64_t scene_generation) override;

private:
    snapshot::SaveSnapshotAdapter saves_;
    std::filesystem::path game_root_;
    std::mutex context_mutex_;
    std::size_t house_cat_count_{};
    std::size_t available_room_count_{};
    std::optional<RuntimeHouseState> runtime_state_;
    std::uint64_t room_mapping_generation_{};
    std::optional<std::unordered_map<
        snapshot::RoomId, RuntimePointer>> room_mapping_;
};

}  // namespace autocattery::ui
