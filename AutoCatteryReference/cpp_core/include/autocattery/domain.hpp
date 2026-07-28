#pragma once
#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace autocattery {
using CatId = std::uint64_t;
using RoomId = std::uint64_t;
enum class TriState { Unknown, No, Yes };
enum class LifeStage { Unknown, Kitten, Adult, Senior, Dead };
enum class ProtectionLevel { None, NoCull, NoMove, NoCullOrMove, FullyUnmanaged };
enum class CatRole { CombatRecommended, BreedingCore, BreedingReserve, GeneralReserve, CullCandidate, ProtectedUnmanaged, Ineligible };

struct StatBlock {
  std::optional<int> strength, dexterity, constitution, intelligence, speed, luck;
};
struct CatSnapshot {
  CatId id{}; std::string display_name; LifeStage life_stage{LifeStage::Unknown}; int age_days{-1};
  StatBlock base_stats; std::vector<std::string> abilities, passives, mutations, disorders;
  std::vector<CatId> parents, compatible_breeding_ids; RoomId room_id{};
  TriState injured{TriState::Unknown}, available_for_combat{TriState::Unknown}, available_for_breeding{TriState::Unknown};
  TriState player_favorite{TriState::Unknown}, game_locked{TriState::Unknown};
  ProtectionLevel protection{ProtectionLevel::None};
};
struct RoomSnapshot {
  RoomId id{}; std::string room_type; std::vector<CatId> residents; std::optional<int> hard_capacity;
  int soft_capacity{4}; std::optional<int> comfort, stimulation; bool allows_breeding{false};
  bool allows_kittens{true}; bool is_special_room{false}; bool player_locked{false};
};
struct HouseSnapshot {
  std::uint64_t snapshot_id{}, scene_generation{}; int game_day{-1}; std::string game_build_id;
  std::vector<CatSnapshot> cats; std::vector<RoomSnapshot> rooms;
};
struct ScoreWeights {
  double strength{1}, dexterity{1}, constitution{1}, intelligence{1}, speed{1}, luck{1};
};
struct CombatConfig {
  int recommended_count{8}; double minimum_score{0}; bool exclude_kittens{true}, exclude_injured{false};
  ScoreWeights stats{}; double ability_count_weight{2}, passive_count_weight{1.5}, mutation_default_weight{0.5};
  double disorder_default_penalty{2}, injury_penalty{5}, missing_field_penalty{0.25};
  std::unordered_map<std::string,double> ability_overrides, passive_overrides, mutation_overrides, disorder_overrides;
};
struct BreedingConfig {
  int core_breeders{4}, reserve_breeders{4}; ScoreWeights stats{}; double ability_default_weight{1};
  double mutation_default_weight{1}, disorder_default_penalty{3}, missing_field_penalty{0.25};
  std::unordered_map<std::string,double> rare_mutation_weights;
};
struct ClassificationConfig {
  bool combat_priority_over_breeding{false}; int minimum_combat_pool{4}, minimum_breeding_pool{4}, minimum_general_reserve{4};
  double never_cull_if_data_confidence_below{0.85};
};
struct RoomPlanningConfig {
  int default_soft_capacity{4}, max_soft_overflow_per_room{2}; bool allow_soft_overflow{true};
  bool never_exceed_known_hard_capacity{true}, prefer_single_combat_staging_room{true};
  bool keep_kittens_separate_when_possible{true}, allow_partial_plan{true};
};
struct AlgorithmConfig { CombatConfig combat; BreedingConfig breeding; ClassificationConfig classification; RoomPlanningConfig rooms; };

struct ScoreComponent { std::string key; double raw_value{}, weight{}, contribution{}; bool missing{false}; std::string explanation; };
struct ScoreResult { CatId cat_id{}; bool eligible{false}; double score{}, confidence{}; std::vector<std::string> exclusion_reasons; std::vector<ScoreComponent> components; };
struct CatDecision {
  CatId cat_id{}; CatRole primary_role{CatRole::Ineligible}; double combat_score{}, breeding_score{}, confidence{};
  ProtectionLevel protection{ProtectionLevel::None}; bool combat_recommended{false}, combat_pool_protected{false};
  bool breeding_core{false}, breeding_reserve{false}, breeding_pool_protected{false}, destructive_action_allowed{false};
  std::vector<std::string> reasons;
};
struct ClassificationPlan { std::uint64_t snapshot_id{}; std::vector<CatDecision> decisions; std::vector<std::string> warnings; };
struct PlannedMove { CatId cat_id{}; RoomId from_room{}, to_room{}; std::string reason; int priority{}; };
struct RoomPlan { std::uint64_t snapshot_id{}; std::vector<PlannedMove> moves; std::vector<CatId> capacity_relief_culls, unplaced_cats; std::vector<std::string> warnings; bool fully_satisfied{false}; };

inline bool IsYes(TriState v) noexcept { return v == TriState::Yes; }
inline bool IsNo(TriState v) noexcept { return v == TriState::No; }
inline bool CanCull(ProtectionLevel p) noexcept { return p == ProtectionLevel::None || p == ProtectionLevel::NoMove; }
inline bool CanMove(ProtectionLevel p) noexcept { return p == ProtectionLevel::None || p == ProtectionLevel::NoCull; }
} // namespace autocattery
