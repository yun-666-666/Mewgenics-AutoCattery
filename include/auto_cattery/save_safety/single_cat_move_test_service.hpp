#pragma once

#include <chrono>
#include <filesystem>
#include <string>

#include "auto_cattery/execution/backup_service.hpp"
#include "auto_cattery/save_safety/atomic_file_replace.hpp"
#include "auto_cattery/save_safety/game_build_gate.hpp"
#include "auto_cattery/save_safety/game_process_probe.hpp"
#include "auto_cattery/save_safety/test_copy_house_state_store.hpp"

namespace autocattery::save_safety {

struct SingleCatMoveTestRequest {
    std::filesystem::path test_root;
    std::filesystem::path test_save;
    std::filesystem::path game_executable;
    std::string operation_id;
    std::size_t moved_index{};
    std::size_t placement_source_index{};
    bool development_test_enabled{};
    std::chrono::milliseconds stable_window{150};
};

struct SingleCatMoveTestOutcome {
    execution::BackupArtifact backup;
    std::string build_identity;
    std::string original_room;
    std::string target_room;
    bool independent_readback_verified{};
};

class SingleCatMoveTestService final {
public:
    SingleCatMoveTestService(
        const IGameBuildGate& build_gate,
        IGameProcessProbe& processes,
        IAtomicFileReplacer& replacer,
        const TestCopyHouseStateStore& store);

    [[nodiscard]] Result<SingleCatMoveTestOutcome> MoveOne(
        const SingleCatMoveTestRequest& request);

private:
    Result<void> RollBackReadbackFailure(
        const SingleCatMoveTestRequest& request,
        const std::filesystem::path& target,
        execution::BackupService& backups) const;

    const IGameBuildGate& build_gate_;
    IGameProcessProbe& processes_;
    IAtomicFileReplacer& replacer_;
    const TestCopyHouseStateStore& store_;
};

}  // namespace autocattery::save_safety
