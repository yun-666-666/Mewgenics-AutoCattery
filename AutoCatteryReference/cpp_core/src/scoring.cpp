#include "autocattery/scoring.hpp"
#include <cmath>
#include <stdexcept>

namespace autocattery {
namespace {

double Finite(double value) {
  if (!std::isfinite(value)) {
    throw std::invalid_argument("non-finite scoring weight");
  }
  return value;
}

std::vector<std::pair<std::string, std::optional<int>>> Stats(const StatBlock& stats) {
  return {
      {"strength", stats.strength},
      {"dexterity", stats.dexterity},
      {"constitution", stats.constitution},
      {"intelligence", stats.intelligence},
      {"speed", stats.speed},
      {"luck", stats.luck},
  };
}

double Weight(const ScoreWeights& weights, const std::string& key) {
  if (key == "strength") return weights.strength;
  if (key == "dexterity") return weights.dexterity;
  if (key == "constitution") return weights.constitution;
  if (key == "intelligence") return weights.intelligence;
  if (key == "speed") return weights.speed;
  return weights.luck;
}

double Override(const std::unordered_map<std::string, double>& values,
                const std::string& key,
                double fallback) {
  const auto it = values.find(key);
  return it == values.end() ? fallback : it->second;
}

void AddItems(ScoreResult& result,
              const std::vector<std::string>& items,
              const std::string& prefix,
              double default_weight,
              const std::unordered_map<std::string, double>& overrides,
              double sign) {
  for (const auto& item : items) {
    const double weight = Finite(Override(overrides, item, default_weight)) * sign;
    result.score += weight;
    result.components.push_back({prefix + ":" + item, 1, weight, weight, false, prefix});
  }
}

int StatSum(const CatSnapshot& cat) {
  int sum = 0;
  for (const auto& [_, value] : Stats(cat.base_stats)) {
    if (value) sum += *value;
  }
  return sum;
}

}  // namespace

ScoreResult ScoreCombat(const CatSnapshot& cat, const CombatConfig& config) {
  ScoreResult result;
  result.cat_id = cat.id;
  if (cat.life_stage == LifeStage::Dead) result.exclusion_reasons.push_back("dead");
  if (config.exclude_kittens && cat.life_stage == LifeStage::Kitten) result.exclusion_reasons.push_back("kitten");
  if (IsNo(cat.available_for_combat)) result.exclusion_reasons.push_back("not available for combat");
  if (config.exclude_injured && IsYes(cat.injured)) result.exclusion_reasons.push_back("injured");

  int observed = 0;
  for (const auto& [key, value] : Stats(cat.base_stats)) {
    const double weight = Finite(Weight(config.stats, key));
    if (value) {
      ++observed;
      const double contribution = *value * weight;
      result.score += contribution;
      result.components.push_back({key, double(*value), weight, contribution, false, "base stat"});
    } else {
      result.score -= config.missing_field_penalty;
      result.components.push_back({key, 0, weight, -config.missing_field_penalty, true, "missing field penalty"});
    }
  }

  AddItems(result, cat.abilities, "ability", config.ability_count_weight, config.ability_overrides, 1);
  AddItems(result, cat.passives, "passive", config.passive_count_weight, config.passive_overrides, 1);
  AddItems(result, cat.mutations, "mutation", config.mutation_default_weight, config.mutation_overrides, 1);
  AddItems(result, cat.disorders, "disorder", config.disorder_default_penalty, config.disorder_overrides, -1);
  if (IsYes(cat.injured)) {
    result.score -= config.injury_penalty;
    result.components.push_back({"injury", 1, -config.injury_penalty, -config.injury_penalty, false, "injury penalty"});
  }
  result.confidence = observed / 6.0;
  result.eligible = result.exclusion_reasons.empty() && result.score >= config.minimum_score;
  return result;
}

ScoreResult ScoreBreeding(const CatSnapshot& cat, const BreedingConfig& config) {
  ScoreResult result;
  result.cat_id = cat.id;
  if (cat.life_stage == LifeStage::Dead) result.exclusion_reasons.push_back("dead");
  if (cat.life_stage == LifeStage::Kitten) result.exclusion_reasons.push_back("kitten");
  if (IsNo(cat.available_for_breeding)) result.exclusion_reasons.push_back("not available for breeding");

  int observed = 0;
  for (const auto& [key, value] : Stats(cat.base_stats)) {
    const double weight = Finite(Weight(config.stats, key));
    if (value) {
      ++observed;
      const double contribution = *value * weight;
      result.score += contribution;
      result.components.push_back({key, double(*value), weight, contribution, false, "base stat potential"});
    } else {
      result.score -= config.missing_field_penalty;
      result.components.push_back({key, 0, weight, -config.missing_field_penalty, true, "missing field penalty"});
    }
  }
  for (const auto& item : cat.abilities) {
    result.score += config.ability_default_weight;
    result.components.push_back({"ability:" + item, 1, config.ability_default_weight, config.ability_default_weight, false, "inheritance value"});
  }
  for (const auto& item : cat.mutations) {
    const double weight = Override(config.rare_mutation_weights, item, config.mutation_default_weight);
    result.score += weight;
    result.components.push_back({"mutation:" + item, 1, weight, weight, false, "gene value"});
  }
  for (const auto& item : cat.disorders) {
    result.score -= config.disorder_default_penalty;
    result.components.push_back({"disorder:" + item, 1, -config.disorder_default_penalty, -config.disorder_default_penalty, false, "disorder risk"});
  }
  result.confidence = observed / 6.0;
  result.eligible = result.exclusion_reasons.empty();
  return result;
}

std::vector<ScoreResult> RankScores(std::vector<ScoreResult> results, const HouseSnapshot& house) {
  std::unordered_map<CatId, int> stat_sums;
  for (const auto& cat : house.cats) stat_sums[cat.id] = StatSum(cat);
  std::sort(results.begin(), results.end(), [&](const auto& left, const auto& right) {
    if (left.eligible != right.eligible) return left.eligible > right.eligible;
    if (left.score != right.score) return left.score > right.score;
    if (left.confidence != right.confidence) return left.confidence > right.confidence;
    if (stat_sums[left.cat_id] != stat_sums[right.cat_id]) return stat_sums[left.cat_id] > stat_sums[right.cat_id];
    return left.cat_id < right.cat_id;
  });
  return results;
}

}  // namespace autocattery
