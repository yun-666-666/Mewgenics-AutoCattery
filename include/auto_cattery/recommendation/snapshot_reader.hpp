#pragma once

#include <filesystem>
#include <string>

#include "auto_cattery/error.hpp"
#include "auto_cattery/workflow/recommendation_snapshot_writer.hpp"

namespace autocattery::recommendation {

enum class SnapshotReadStatus {
    Missing,
    Available,
    Rejected
};

struct SnapshotReadResult {
    SnapshotReadStatus status{SnapshotReadStatus::Missing};
    workflow::RecommendationSnapshot snapshot;
    std::string reason;
};

[[nodiscard]] std::filesystem::path RecommendationSidecarPath(
    const std::filesystem::path& mod_root);

[[nodiscard]] SnapshotReadResult ReadRecommendationSnapshot(
    const std::filesystem::path& sidecar_path);

}  // namespace autocattery::recommendation
