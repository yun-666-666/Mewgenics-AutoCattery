#include "auto_cattery/workflow/recommendation_snapshot_writer.hpp"

#include <windows.h>

#include <fstream>
#include <iomanip>
#include <sstream>
#include <nlohmann/json.hpp>

namespace autocattery::workflow {

std::string RecommendationPayloadChecksum(std::string_view payload_text) {
  std::uint64_t value = 14695981039346656037ULL;
  for (const unsigned char byte : payload_text) {
    value = (value ^ byte) * 1099511628211ULL;
  }
  std::ostringstream output;
  output << std::hex << std::setfill('0') << std::setw(16) << value;
  return output.str();
}

AtomicRecommendationSnapshotWriter::AtomicRecommendationSnapshotWriter(
    std::filesystem::path mod_data_root)
    : mod_data_root_(std::move(mod_data_root)) {}

Result<void> AtomicRecommendationSnapshotWriter::WriteCommitted(
    const RecommendationSnapshot &snapshot) {
  if (mod_data_root_.empty() || snapshot.source_snapshot_id == 0 ||
      snapshot.combat_algorithm_version.empty() ||
      snapshot.config_digest.empty()) {
    return {ErrorCode::ConfigInvalid,
            "recommendation snapshot input is invalid"};
  }

  nlohmann::json payload{
      {"schema_version", 1},
      {"created_on_game_day", snapshot.game_day},
      {"source_snapshot_id", snapshot.source_snapshot_id},
      {"combat_algorithm_version", snapshot.combat_algorithm_version},
      {"config_digest", snapshot.config_digest},
      {"recommended", nlohmann::json::array()}};
  for (const auto &entry : snapshot.recommended) {
    payload["recommended"].push_back({{"cat_id", std::to_string(entry.cat_id)},
                                      {"rank", entry.rank},
                                      {"score", entry.score},
                                      {"confidence", entry.confidence}});
  }
  const auto payload_text = payload.dump();
  const nlohmann::json envelope{
      {"checksum", RecommendationPayloadChecksum(payload_text)},
                                {"payload", payload}};

  std::error_code error;
  std::filesystem::create_directories(mod_data_root_, error);
  if (error) {
    return {ErrorCode::WriteConflict, "could not create MOD data root"};
  }
  const auto root = std::filesystem::weakly_canonical(mod_data_root_, error);
  if (error) {
    return {ErrorCode::WriteConflict, "MOD data root is invalid"};
  }
  const auto target = root / L"recommendations.json";
  const auto temporary = root / L"recommendations.json.tmp";
  std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
  output << envelope.dump(2);
  output.flush();
  if (!output) {
    output.close();
    std::filesystem::remove(temporary, error);
    return {ErrorCode::WriteConflict, "sidecar temporary write failed"};
  }
  output.close();
  if (!MoveFileExW(temporary.c_str(), target.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    std::filesystem::remove(temporary, error);
    return {ErrorCode::WriteConflict, "sidecar atomic replace failed"};
  }
  return {};
}

RecommendationWriteResult
WriteRecommendationAfterOutcome(const OrganizeOutcome &outcome,
                                const RecommendationSnapshot &snapshot,
                                IRecommendationSnapshotWriter &writer) {
  if (outcome.state != WorkflowState::Completed ||
      outcome.failure_reason != WorkflowFailureReason::None ||
      !outcome.game_data_modified) {
    return {
        RecommendationWriteStatus::Skipped,
        "Recommendation snapshot skipped because execution did not commit."};
  }
  const auto written = writer.WriteCommitted(snapshot);
  if (!written) {
    return {
        RecommendationWriteStatus::Warning,
        "Organize committed, but the recommendation snapshot is unavailable."};
  }
  return {RecommendationWriteStatus::Written,
          "Recommendation snapshot written."};
}

} // namespace autocattery::workflow
