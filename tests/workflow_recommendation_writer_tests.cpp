#include "auto_cattery/workflow/recommendation_snapshot_writer.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

class RecommendationWriterFake final
    : public workflow::IRecommendationSnapshotWriter {
public:
  Result<void>
  WriteCommitted(const workflow::RecommendationSnapshot &) override {
    ++calls;
    return failure ? Result<void>{ErrorCode::WriteConflict, "synthetic failure"}
                   : Result<void>{};
  }

  bool failure{};
  int calls{};
};

} // namespace

void RunWorkflowRecommendationWriterTests() {
  const auto nonce =
      std::chrono::steady_clock::now().time_since_epoch().count();
  const auto root =
      std::filesystem::temp_directory_path() /
      ("auto_cattery_phase11_recommendation_" + std::to_string(nonce));
  workflow::AtomicRecommendationSnapshotWriter writer(root);

  workflow::RecommendationSnapshot snapshot;
  snapshot.game_day = 12;
  snapshot.source_snapshot_id = 44;
  snapshot.combat_algorithm_version = "fixture-v1";
  snapshot.config_digest = "config-digest";
  snapshot.recommended = {{101, 1, 91.5, 1.0}};
  AC_CHECK(static_cast<bool>(writer.WriteCommitted(snapshot)));
  AC_CHECK(std::filesystem::exists(root / "recommendations.json"));
  AC_CHECK(!std::filesystem::exists(root / "recommendations.json.tmp"));

  std::ifstream input(root / "recommendations.json");
  const std::string contents{std::istreambuf_iterator<char>(input),
                             std::istreambuf_iterator<char>()};
  AC_CHECK(contents.find("\"checksum\"") != std::string::npos);
  AC_CHECK(contents.find("\"schema_version\": 1") != std::string::npos);
  AC_CHECK(contents.find("\"cat_id\": \"101\"") != std::string::npos);
  input.close();

  workflow::RecommendationSnapshot invalid;
  AC_CHECK(!static_cast<bool>(writer.WriteCommitted(invalid)));

  RecommendationWriterFake fake;
  workflow::OrganizeOutcome outcome;
  outcome.state = workflow::WorkflowState::Failed;
  AC_CHECK(workflow::WriteRecommendationAfterOutcome(outcome, snapshot, fake)
               .status == workflow::RecommendationWriteStatus::Skipped);
  AC_CHECK(fake.calls == 0);
  outcome.state = workflow::WorkflowState::Cancelled;
  (void)workflow::WriteRecommendationAfterOutcome(outcome, snapshot, fake);
  AC_CHECK(fake.calls == 0);
  outcome.state = workflow::WorkflowState::Completed;
  outcome.game_data_modified = true;
  AC_CHECK(workflow::WriteRecommendationAfterOutcome(outcome, snapshot, fake)
               .status == workflow::RecommendationWriteStatus::Written);
  AC_CHECK(fake.calls == 1);
  fake.failure = true;
  AC_CHECK(workflow::WriteRecommendationAfterOutcome(outcome, snapshot, fake)
               .status == workflow::RecommendationWriteStatus::Warning);
  AC_CHECK(fake.calls == 2);

  std::error_code cleanup_error;
  std::filesystem::remove_all(root, cleanup_error);
  AC_CHECK(!cleanup_error);
}

} // namespace autocattery::tests
