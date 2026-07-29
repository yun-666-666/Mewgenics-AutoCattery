#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "auto_cattery/error.hpp"
#include "auto_cattery/snapshot/domain.hpp"
#include "auto_cattery/workflow/domain.hpp"

namespace autocattery::workflow {

struct RecommendationEntry {
  snapshot::CatId cat_id{};
  std::size_t rank{};
  double score{};
  double confidence{};
};

struct RecommendationSnapshot {
  std::int64_t game_day{};
  std::uint64_t source_snapshot_id{};
  std::string combat_algorithm_version;
  std::string config_digest;
  std::vector<RecommendationEntry> recommended;
};

class IRecommendationSnapshotWriter {
public:
  virtual ~IRecommendationSnapshotWriter() = default;
  virtual Result<void>
  WriteCommitted(const RecommendationSnapshot &snapshot) = 0;
};

class AtomicRecommendationSnapshotWriter final
    : public IRecommendationSnapshotWriter {
public:
  explicit AtomicRecommendationSnapshotWriter(
      std::filesystem::path mod_data_root);
  Result<void> WriteCommitted(const RecommendationSnapshot &snapshot) override;

private:
  std::filesystem::path mod_data_root_;
};

enum class RecommendationWriteStatus { Skipped, Written, Warning };

struct RecommendationWriteResult {
  RecommendationWriteStatus status{RecommendationWriteStatus::Skipped};
  std::string message;
};

[[nodiscard]] RecommendationWriteResult
WriteRecommendationAfterOutcome(const OrganizeOutcome &outcome,
                                const RecommendationSnapshot &snapshot,
                                IRecommendationSnapshotWriter &writer);

} // namespace autocattery::workflow
