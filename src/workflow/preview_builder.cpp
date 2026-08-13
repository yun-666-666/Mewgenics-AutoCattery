#include "auto_cattery/workflow/preview_builder.hpp"

#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include "auto_cattery/breeding/breeding_ranker.hpp"
#include "auto_cattery/classification/classifier.hpp"
#include "auto_cattery/classification/protection_adapter.hpp"
#include "move_only_protection.hpp"
#include "auto_cattery/execution/plan_sealer.hpp"
#include "auto_cattery/logger.hpp"
#include "auto_cattery/room_planning/balanced_move_only_planner.hpp"
#include "auto_cattery/room_planning/capability_adapter.hpp"
#include "auto_cattery/room_planning/planner.hpp"
#include "auto_cattery/scoring/combat_ranker.hpp"
#include "auto_cattery/workflow/digests.hpp"

namespace autocattery::workflow {
namespace {

std::string Summary(const OrganizePreview &preview) {
  std::ostringstream output;
  output << "Organize preview: cats=" << preview.cat_count
         << ", rooms=" << preview.room_count;
  if (preview.capability == WorkflowCapability::MoveOnly) {
    output << ", potential_top="
           << preview.recommended_combat_count;
  } else {
    output << ", combat=" << preview.recommended_combat_count
           << ", breeding_core=" << preview.breeding_core_count
           << ", breeding_reserve=" << preview.breeding_reserve_count
           << ", protected=" << preview.protected_count;
  }
  output << ", moves=" << preview.planned_move_count
         << ", capacity_relief=" << preview.capacity_relief_count
         << ", unplaced=" << preview.unplaced_count;
  if (preview.capability == WorkflowCapability::MoveOnly) {
    output << "; click Auto-Organize again to apply moves only.";
  } else {
    output << "; no game data changed; real execution unavailable.";
  }
  return output.str();
}

void AddUnique(std::vector<std::string> &values, const std::string &value) {
  if (std::ranges::find(values, value) == values.end()) {
    values.push_back(value);
  }
}

void EnsureCurrentBuildRooms(snapshot::HouseSnapshot &snapshot) {
  for (const auto id : {"Floor1_Large", "Attic"}) {
    if (std::ranges::none_of(
            snapshot.rooms,
            [id](const auto &room) { return room.id == id; })) {
      snapshot.rooms.push_back({.id = id});
    }
  }
}

std::string ObservedRoomSummary(
    const snapshot::HouseSnapshot &snapshot) {
  std::unordered_map<snapshot::RoomId, std::size_t> residents;
  for (const auto &cat : snapshot.cats) {
    if (cat.room_id) {
      ++residents[*cat.room_id];
    }
  }
  std::ostringstream output;
  bool first = true;
  for (const auto &room : snapshot.rooms) {
    if (!first) {
      output << ", ";
    }
    first = false;
    output << room.id << '(' << residents[room.id] << ')';
  }
  if (first) {
    output << "none";
  }
  return output.str();
}

std::string SexSummary(const snapshot::HouseSnapshot &snapshot) {
  std::size_t female{};
  std::size_t male{};
  std::size_t unknown{};
  for (const auto &cat : snapshot.cats) {
    switch (cat.sex) {
    case snapshot::CatSex::Female:
      ++female;
      break;
    case snapshot::CatSex::Male:
      ++male;
      break;
    case snapshot::CatSex::Unknown:
      ++unknown;
      break;
    }
  }
  return "female=" + std::to_string(female) +
         ", male=" + std::to_string(male) +
         ", unknown=" + std::to_string(unknown);
}

snapshot::HouseSnapshot PotentialScoringSnapshot(
    const snapshot::HouseSnapshot &source) {
  auto scoring = source;
  for (auto &cat : scoring.cats) {
    cat.life_stage = snapshot::LifeStage::Adult;
    cat.available_for_combat = snapshot::TriState::Yes;
    cat.available_for_breeding = snapshot::TriState::Yes;
    cat.injured = snapshot::TriState::No;
  }
  return scoring;
}

void ApplyPotentialOnlyRoles(
    classification::ClassificationPlan &classification,
    const scoring::CombatRanking &potential) {
  const std::unordered_set<snapshot::CatId> top(
      potential.recommended_cat_ids.begin(),
      potential.recommended_cat_ids.end());
  for (auto &decision : classification.decisions) {
    const bool selected = top.contains(decision.cat_id);
    decision.primary_role =
        selected
            ? classification::CatRole::CombatRecommended
            : classification::CatRole::GeneralReserve;
    decision.combat_recommended = selected;
    decision.combat_pool_protected = selected;
    decision.breeding_core = false;
    decision.breeding_reserve = false;
    decision.breeding_pool_protected = false;
    decision.preview_cull_candidate = false;
    decision.destructive_action_allowed = false;
  }
  classification.quality_cull_candidates.clear();
  classification.capacity_relief_candidates.clear();
}

} // namespace

PreviewBuilder::PreviewBuilder(
    snapshot::IGameReadAdapter &read_adapter,
    Config config,
    std::filesystem::path protection_sidecar_path)
    : read_adapter_(read_adapter), config_(std::move(config)),
      protection_sidecar_path_(std::move(protection_sidecar_path)) {}

protection::ProtectionDigest PreviewBuilder::CaptureProtectionDigest(
    const snapshot::HouseSnapshot &snapshot,
    WorkflowCapability capability) const {
  if (capability == WorkflowCapability::MoveOnly) {
    return detail::BuildMoveOnlyProtections(
        snapshot, protection_sidecar_path_).digest;
  }
  protection::ProtectionSidecar sidecar;
  classification::NativeProtectionFactsByCat native;
  classification::IdentityTokenByCat identities;
  auto safety = classification::BuildCullSafetyFacts(
      snapshot, native, sidecar, identities);
  std::vector<protection::ProtectionDecision> decisions;
  decisions.reserve(snapshot.cats.size());
  for (const auto &cat : snapshot.cats) {
    auto decision = *safety.at(cat.id).policy_decision;
    decisions.push_back(std::move(decision));
  }
  return protection::BuildDigest(decisions);
}

Result<PreviewBundle> PreviewBuilder::Build(std::uint64_t scene_generation,
                                            WorkflowCapability capability,
                                            WorkflowStateMachine &state,
                                            bool include_all_purpose_rooms) const {
  auto captured = read_adapter_.CaptureHouseSnapshot(scene_generation);
  if (!captured) {
    state.Fail();
    return {{}, captured.code, captured.message};
  }
  if (capability == WorkflowCapability::MoveOnly) {
    Logger::Instance().Write(
        LogLevel::Info,
        "RoomDetection",
        "AC14310",
        "Observed save rooms: " + ObservedRoomSummary(captured.value) +
            "; voice sex: " + SexSummary(captured.value));
    EnsureCurrentBuildRooms(captured.value);
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

  auto combat_config = config_.combat_scoring;
  auto breeding_config = config_.breeding_scoring;
  if (capability == WorkflowCapability::MoveOnly) {
    combat_config.require_confirmed_eligibility = false;
    breeding_config.require_confirmed_eligibility = false;
  }
  const auto scoring_snapshot =
      capability == WorkflowCapability::MoveOnly
          ? PotentialScoringSnapshot(captured.value)
          : captured.value;
  auto combat = scoring::RankCombatCats(scoring_snapshot, combat_config);
  auto breeding =
      breeding::RankBreedingCats(captured.value, breeding_config);
  if (!combat) {
    state.Fail();
    return {{}, combat.code, combat.message};
  }
  if (!breeding) {
    state.Fail();
    return {{}, breeding.code, breeding.message};
  }
  classification::CullSafetyFactsByCat safety;
  std::vector<protection::ProtectionDecision> protections;
  protection::ProtectionDigest protection_digest;
  if (capability == WorkflowCapability::MoveOnly) {
    auto protected_set = detail::BuildMoveOnlyProtections(
        captured.value, protection_sidecar_path_);
    safety = std::move(protected_set.safety);
    protections = std::move(protected_set.decisions);
    protection_digest = std::move(protected_set.digest);
  } else {
    protection::ProtectionSidecar sidecar;
    classification::NativeProtectionFactsByCat native;
    classification::IdentityTokenByCat identities;
    safety = classification::BuildCullSafetyFacts(
        captured.value, native, sidecar, identities);
    protections.reserve(captured.value.cats.size());
    for (const auto &cat : captured.value.cats) {
      protections.push_back(*safety.at(cat.id).policy_decision);
    }
    protection_digest = protection::BuildDigest(protections);
  }
  auto classified = classification::ClassifyCats(
      captured.value, combat.value, breeding.value, breeding_config,
      config_.classification, safety);
  if (!classified || !state.PlanningComplete()) {
    state.Fail();
    return {{},
            classified ? ErrorCode::WriteConflict : classified.code,
            classified ? "workflow state changed" : classified.message};
  }
  if (capability == WorkflowCapability::MoveOnly) {
    ApplyPotentialOnlyRoles(classified.value, combat.value);
  }

  const auto capabilities =
      capability == WorkflowCapability::MoveOnly
          ? (include_all_purpose_rooms
                 ? room_planning::BuildCurrentBuildPurposeRoomCapabilities(
                       captured.value)
                 : room_planning::BuildCurrentBuildMoveRoomCapabilities(
                       captured.value))
          : room_planning::BuildConservativeRoomCapabilities(
                captured.value);
  const room_planning::RoomPlanningInput planning_input{
      captured.value, classified.value, protections, capabilities,
      protection_digest, protection_digest};
  auto room_plan =
      capability == WorkflowCapability::MoveOnly
          ? room_planning::PlanCurrentBuildBalancedMoveOnlyRooms(
                planning_input, config_.room_planning)
          : room_planning::PlanRooms(
                planning_input, config_.room_planning);
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
  if (capability == WorkflowCapability::PreviewOnly) {
    AddUnique(preview.warnings, "preview-only-no-verified-write-adapter");
  } else {
    AddUnique(preview.warnings, "move-only-cull-unavailable");
  }
  preview.summary = Summary(preview);
  return {std::move(bundle)};
}

} // namespace autocattery::workflow
