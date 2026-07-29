#include "auto_cattery/workflow/preview_builder.hpp"

#include <algorithm>
#include <sstream>

#include "auto_cattery/breeding/breeding_ranker.hpp"
#include "auto_cattery/classification/classifier.hpp"
#include "auto_cattery/classification/protection_adapter.hpp"
#include "auto_cattery/execution/plan_sealer.hpp"
#include "auto_cattery/room_planning/capability_adapter.hpp"
#include "auto_cattery/room_planning/planner.hpp"
#include "auto_cattery/scoring/combat_ranker.hpp"
#include "auto_cattery/workflow/digests.hpp"

namespace autocattery::workflow {
namespace {

std::string Summary(const OrganizePreview &preview) {
  std::ostringstream output;
  output << "Read-only organize preview: cats=" << preview.cat_count
         << ", rooms=" << preview.room_count
         << ", combat=" << preview.recommended_combat_count
         << ", breeding_core=" << preview.breeding_core_count
         << ", breeding_reserve=" << preview.breeding_reserve_count
         << ", protected=" << preview.protected_count
         << ", moves=" << preview.planned_move_count
         << ", capacity_relief=" << preview.capacity_relief_count
         << ", unplaced=" << preview.unplaced_count
         << "; no game data changed; real execution unavailable.";
  return output.str();
}

void AddUnique(std::vector<std::string> &values, const std::string &value) {
  if (std::ranges::find(values, value) == values.end()) {
    values.push_back(value);
  }
}

} // namespace

PreviewBuilder::PreviewBuilder(snapshot::IGameReadAdapter &read_adapter,
                               Config config)
    : read_adapter_(read_adapter), config_(std::move(config)) {}

Result<PreviewBundle> PreviewBuilder::Build(std::uint64_t scene_generation,
                                            WorkflowCapability capability,
                                            WorkflowStateMachine &state) const {
  auto captured = read_adapter_.CaptureHouseSnapshot(scene_generation);
  if (!captured) {
    state.Fail();
    return {{}, captured.code, captured.message};
  }
  const auto validation = snapshot::Validate(captured.value);
  if (!validation.Valid()) {
    state.Fail();
    return {
        {}, ErrorCode::SnapshotInvalid, "read-only snapshot failed validation"};
  }
  if (!state.ScoringComplete()) {
    state.Fail();
    return {{}, ErrorCode::WriteConflict, "workflow state changed"};
  }

  auto combat = scoring::RankCombatCats(captured.value, config_.combat_scoring);
  auto breeding =
      breeding::RankBreedingCats(captured.value, config_.breeding_scoring);
  if (!combat) {
    state.Fail();
    return {{}, combat.code, combat.message};
  }
  if (!breeding) {
    state.Fail();
    return {{}, breeding.code, breeding.message};
  }

  protection::ProtectionSidecar sidecar;
  classification::NativeProtectionFactsByCat native;
  classification::IdentityTokenByCat identities;
  auto safety = classification::BuildCullSafetyFacts(captured.value, native,
                                                     sidecar, identities);
  std::vector<protection::ProtectionDecision> protections;
  protections.reserve(captured.value.cats.size());
  for (const auto &cat : captured.value.cats) {
    protections.push_back(*safety.at(cat.id).policy_decision);
  }
  const auto protection_digest = protection::BuildDigest(protections);
  auto classified = classification::ClassifyCats(
      captured.value, combat.value, breeding.value, config_.breeding_scoring,
      config_.classification, safety);
  if (!classified || !state.PlanningComplete()) {
    state.Fail();
    return {{},
            classified ? ErrorCode::WriteConflict : classified.code,
            classified ? "workflow state changed" : classified.message};
  }

  const auto capabilities =
      room_planning::BuildConservativeRoomCapabilities(captured.value);
  auto room_plan = room_planning::PlanRooms(
      {captured.value, classified.value, protections, capabilities,
       protection_digest, protection_digest},
      config_.room_planning);
  if (!state.PlanningComplete()) {
    state.Fail();
    return {{}, ErrorCode::WriteConflict, "workflow state changed"};
  }

  PreviewBundle bundle;
  bundle.snapshot = std::move(captured.value);
  bundle.classification = std::move(classified.value);
  bundle.protections = std::move(protections);
  bundle.room_plan = std::move(room_plan);
  auto &preview = bundle.preview;
  preview.capability = capability;
  preview.expiration.expires_at =
      std::chrono::steady_clock::now() +
      std::chrono::seconds(config_.workflow.preview_ttl_seconds);
  preview.bindings = {bundle.snapshot.scene_generation,
                      bundle.snapshot.game_day,
                      execution::DigestSnapshotContent(bundle.snapshot),
                      execution::DigestClassification(bundle.classification),
                      std::to_string(protection_digest.value),
                      execution::DigestRoomPlan(bundle.room_plan).value,
                      DigestConfig(config_),
                      DigestCandidateOrder(bundle.classification),
                      DigestPrivateIdentity(bundle.snapshot.source_save_name),
                      "unknown"};
  preview.cat_count = bundle.snapshot.cats.size();
  preview.room_count = bundle.snapshot.rooms.size();
  preview.recommended_combat_count = combat.value.recommended_cat_ids.size();
  for (const auto &decision : bundle.classification.decisions) {
    preview.breeding_core_count += decision.breeding_core ? 1U : 0U;
    preview.breeding_reserve_count += decision.breeding_reserve ? 1U : 0U;
  }
  preview.protected_count = protection_digest.protected_count;
  preview.planned_move_count = bundle.room_plan.moves.size();
  preview.capacity_relief_count =
      bundle.room_plan.capacity_relief_suggestions.size();
  preview.unplaced_count = bundle.room_plan.unplaced_cats.size();
  preview.disposition = bundle.room_plan.disposition;
  preview.fully_satisfied = bundle.room_plan.fully_satisfied;
  preview.game_data_modified = false;
  preview.warnings = bundle.classification.global_warnings;
  for (const auto &warning : bundle.room_plan.warnings) {
    AddUnique(preview.warnings, warning);
  }
  for (const auto &limitation : bundle.room_plan.limitations) {
    AddUnique(preview.warnings, limitation);
  }
  AddUnique(preview.warnings, "preview-only-no-verified-write-adapter");
  preview.summary = Summary(preview);
  return {std::move(bundle)};
}

} // namespace autocattery::workflow
