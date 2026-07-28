#include "autocattery/classification.hpp"
#include "autocattery/scoring.hpp"
#include <set>
#include <unordered_map>

namespace autocattery {
namespace {

ProtectionLevel EffectiveProtection(const CatSnapshot& cat) {
  if (cat.protection == ProtectionLevel::FullyUnmanaged) return cat.protection;
  bool no_cull = cat.protection == ProtectionLevel::NoCull || cat.protection == ProtectionLevel::NoCullOrMove;
  bool no_move = cat.protection == ProtectionLevel::NoMove || cat.protection == ProtectionLevel::NoCullOrMove;
  if (IsYes(cat.game_locked)) {
    no_cull = true;
    no_move = true;
  } else if (IsYes(cat.player_favorite)) {
    no_cull = true;
  }
  if (no_cull && no_move) return ProtectionLevel::NoCullOrMove;
  if (no_cull) return ProtectionLevel::NoCull;
  if (no_move) return ProtectionLevel::NoMove;
  return ProtectionLevel::None;
}

std::set<CatId> Take(const std::vector<CatId>& values, int count) {
  const auto end = values.begin() + std::min<int>(count, values.size());
  return {values.begin(), end};
}

}  // namespace

ClassificationPlan ClassifyCats(const HouseSnapshot& house, const AlgorithmConfig& config) {
  ClassificationPlan output;
  output.snapshot_id = house.snapshot_id;

  std::vector<ScoreResult> combat_results;
  std::vector<ScoreResult> breeding_results;
  for (const auto& cat : house.cats) {
    combat_results.push_back(ScoreCombat(cat, config.combat));
    breeding_results.push_back(ScoreBreeding(cat, config.breeding));
  }
  combat_results = RankScores(std::move(combat_results), house);
  breeding_results = RankScores(std::move(breeding_results), house);

  std::vector<CatId> combat_eligible;
  std::vector<CatId> breeding_eligible;
  for (const auto& result : combat_results) if (result.eligible) combat_eligible.push_back(result.cat_id);
  for (const auto& result : breeding_results) if (result.eligible) breeding_eligible.push_back(result.cat_id);

  const auto combat_recommended = Take(combat_eligible, config.combat.recommended_count);
  const auto combat_pool = Take(combat_eligible, std::max(config.combat.recommended_count, config.classification.minimum_combat_pool));
  const auto breeding_core = Take(breeding_eligible, config.breeding.core_breeders);
  std::set<CatId> breeding_reserve;
  for (int i = config.breeding.core_breeders;
       i < std::min<int>(breeding_eligible.size(), config.breeding.core_breeders + config.breeding.reserve_breeders);
       ++i) {
    breeding_reserve.insert(breeding_eligible[i]);
  }
  const auto breeding_pool = Take(
      breeding_eligible,
      std::max(config.breeding.core_breeders + config.breeding.reserve_breeders,
               config.classification.minimum_breeding_pool));

  std::unordered_map<CatId, ScoreResult> combat_by_id;
  std::unordered_map<CatId, ScoreResult> breeding_by_id;
  for (const auto& result : combat_results) combat_by_id[result.cat_id] = result;
  for (const auto& result : breeding_results) breeding_by_id[result.cat_id] = result;

  std::vector<CatId> unresolved;
  for (const auto& cat : house.cats) {
    CatDecision decision;
    decision.cat_id = cat.id;
    decision.combat_score = combat_by_id[cat.id].score;
    decision.breeding_score = breeding_by_id[cat.id].score;
    decision.confidence = std::min(combat_by_id[cat.id].confidence, breeding_by_id[cat.id].confidence);
    decision.protection = EffectiveProtection(cat);
    decision.combat_recommended = combat_recommended.contains(cat.id);
    decision.combat_pool_protected = combat_pool.contains(cat.id);
    decision.breeding_core = breeding_core.contains(cat.id);
    decision.breeding_reserve = breeding_reserve.contains(cat.id);
    decision.breeding_pool_protected = breeding_pool.contains(cat.id);

    if (decision.protection == ProtectionLevel::FullyUnmanaged) {
      decision.primary_role = CatRole::ProtectedUnmanaged;
    } else if (!combat_by_id[cat.id].eligible && !breeding_by_id[cat.id].eligible) {
      decision.primary_role = CatRole::Ineligible;
    } else if (decision.combat_recommended && decision.breeding_core) {
      decision.primary_role = config.classification.combat_priority_over_breeding
                                  ? CatRole::CombatRecommended
                                  : CatRole::BreedingCore;
    } else if (decision.breeding_core) {
      decision.primary_role = CatRole::BreedingCore;
    } else if (decision.combat_recommended) {
      decision.primary_role = CatRole::CombatRecommended;
    } else if (decision.breeding_reserve) {
      decision.primary_role = CatRole::BreedingReserve;
    } else {
      decision.primary_role = CatRole::GeneralReserve;
      unresolved.push_back(cat.id);
    }
    output.decisions.push_back(decision);
  }

  std::sort(unresolved.begin(), unresolved.end(), [&](CatId left, CatId right) {
    const double left_value = std::max(combat_by_id[left].score, breeding_by_id[left].score);
    const double right_value = std::max(combat_by_id[right].score, breeding_by_id[right].score);
    return left_value == right_value ? left < right : left_value > right_value;
  });
  const auto general_reserve = Take(unresolved, config.classification.minimum_general_reserve);

  std::unordered_map<CatId, const CatSnapshot*> cats;
  for (const auto& cat : house.cats) cats[cat.id] = &cat;
  for (auto& decision : output.decisions) {
    if (std::find(unresolved.begin(), unresolved.end(), decision.cat_id) == unresolved.end()) continue;
    const bool guarded = !CanCull(decision.protection) || decision.combat_pool_protected ||
                         decision.breeding_pool_protected || cats[decision.cat_id]->life_stage == LifeStage::Kitten ||
                         decision.confidence < config.classification.never_cull_if_data_confidence_below;
    if (general_reserve.contains(decision.cat_id) || guarded) {
      decision.primary_role = CatRole::GeneralReserve;
      decision.reasons.push_back("minimum pool, protection, kitten, or confidence guard");
    } else {
      decision.primary_role = CatRole::CullCandidate;
      decision.destructive_action_allowed = true;
      decision.reasons.push_back("outside protected combat/breeding/general pools");
    }
  }
  return output;
}

}  // namespace autocattery
