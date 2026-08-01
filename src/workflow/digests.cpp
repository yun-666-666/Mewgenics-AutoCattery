#include "auto_cattery/workflow/digests.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <vector>

namespace autocattery::workflow {
namespace {

template <class T> void Append(std::ostringstream &output, const T &value) {
  output << value << '|';
}

void AppendOverrides(std::ostringstream &output,
                     const std::unordered_map<std::string, double> &overrides) {
  std::vector<std::pair<std::string, double>> sorted{overrides.begin(),
                                                     overrides.end()};
  std::ranges::sort(sorted);
  for (const auto &[key, value] : sorted) {
    Append(output, key.size());
    Append(output, key);
    Append(output, value);
  }
}

template <class ScoringConfig>
void AppendScoring(std::ostringstream &output, const ScoringConfig &config) {
  Append(output, config.version);
  Append(output, config.minimum_score);
  Append(output, config.minimum_known_stats);
  Append(output, config.require_confirmed_eligibility);
  for (const auto weight : config.stat_weights) {
    Append(output, weight);
  }
  Append(output, config.missing_stat_penalty);
  Append(output, config.active_ability_default_weight);
  Append(output, config.passive_default_weight);
  Append(output, config.disorder_default_penalty);
  AppendOverrides(output, config.active_ability_overrides);
  AppendOverrides(output, config.passive_overrides);
  AppendOverrides(output, config.disorder_overrides);
}

} // namespace

std::string DigestPrivateIdentity(std::string_view value) {
  std::uint64_t hash = 14695981039346656037ULL;
  for (const unsigned char byte : value) {
    hash = (hash ^ byte) * 1099511628211ULL;
  }
  std::ostringstream output;
  output << std::hex << std::setfill('0') << std::setw(16) << hash;
  return output.str();
}

std::string DigestConfig(const Config &config) {
  std::ostringstream canonical;
  Append(canonical, config.schema_version);
  Append(canonical, config.general.version);
  Append(canonical, config.general.mod_enabled);
  Append(canonical, config.general.safe_mode);
  Append(canonical, config.general.log_level);
  Append(canonical, config.general.language);
  Append(canonical, config.safe_mode);
  Append(canonical, config.force_read_only);
  Append(canonical, config.ui.version);
  Append(canonical, config.ui.house_button_enabled);
  Append(canonical, config.ui.embark_button_enabled);
  Append(canonical, config.ui.show_debug_overlay);
  Append(canonical, config.execution_safety.version);
  Append(canonical,
         config.execution_safety.require_preview_before_destructive_actions);
  Append(canonical, config.execution_safety.create_backup_before_apply);
  Append(canonical, config.execution_safety.read_only_mode);
  Append(canonical, config.execution_safety.single_click_execute);
  Append(canonical, config.execution_safety.abort_on_unknown_game_build);
  Append(canonical, config.execution.real_write_adapter_enabled);
  Append(canonical, config.execution.cull_enabled);
  Append(canonical, config.execution.require_quiescent_backup);
  Append(canonical, config.workflow.preview_ttl_seconds);
  AppendScoring(canonical, config.combat_scoring);
  Append(canonical, config.combat_scoring.recommended_count);
  Append(canonical, config.combat_scoring.exclude_kittens);
  Append(canonical, config.combat_scoring.exclude_injured);
  Append(canonical, config.combat_scoring.injury_penalty);
  AppendScoring(canonical, config.breeding_scoring);
  Append(canonical, config.breeding_scoring.core_breeders);
  Append(canonical, config.breeding_scoring.reserve_breeders);
  Append(canonical, config.breeding_scoring.mutation_default_weight);
  Append(canonical, config.breeding_scoring.birth_defect_default_penalty);
  AppendOverrides(canonical, config.breeding_scoring.mutation_overrides);
  AppendOverrides(canonical, config.breeding_scoring.birth_defect_overrides);
  Append(canonical, config.classification.version);
  Append(canonical, config.classification.combat_priority_over_breeding);
  Append(canonical, config.classification.minimum_combat_pool);
  Append(canonical, config.classification.minimum_breeding_pool);
  Append(canonical, config.classification.minimum_general_reserve);
  Append(canonical, config.classification.never_cull_if_data_confidence_below);
  Append(canonical, config.protection.version);
  Append(canonical, config.protection.sidecar_file);
  Append(canonical, config.protection.protect_unknown_native_state);
  Append(canonical, config.protection.require_stable_identity_for_sidecar);
  Append(canonical, config.room_planning.version);
  Append(canonical, config.room_planning.default_soft_capacity);
  Append(canonical, config.room_planning.allow_soft_overflow);
  Append(canonical, config.room_planning.max_soft_overflow_per_room);
  Append(canonical, config.room_planning.never_exceed_known_hard_capacity);
  Append(canonical, config.room_planning.prefer_single_combat_staging_room);
  Append(canonical, config.room_planning.keep_breeding_pairs_together);
  Append(canonical, config.room_planning.avoid_inbreeding_pairs);
  Append(canonical, config.room_planning.keep_kittens_separate_when_possible);
  Append(canonical, config.room_planning.allow_partial_plan);
  Append(canonical, config.recommendation_marker.version);
  Append(canonical, config.recommendation_marker.recommended_count);
  Append(canonical, config.recommendation_marker.show_score);
  Append(canonical, config.recommendation_marker.show_rank);
  Append(canonical, config.recommendation_marker.pulse_top_n);
  Append(canonical, config.recommendation_marker.auto_clear_on_scene_exit);
  Append(canonical, config.recommendation_marker.recompute_if_stale);
  Append(canonical, config.recommendation_marker.never_auto_select);
  Append(canonical, config.diagnostics.version);
  Append(canonical, config.diagnostics.show_debug_overlay);
  Append(canonical, config.diagnostics.export_scene_summary_enabled);
  Append(canonical, config.diagnostics.collect_cat_data);
  return DigestPrivateIdentity(canonical.str());
}

std::string
DigestCandidateOrder(const classification::ClassificationPlan &plan) {
  std::ostringstream canonical;
  for (const auto id : plan.capacity_relief_candidates) {
    Append(canonical, id);
  }
  return DigestPrivateIdentity(canonical.str());
}

} // namespace autocattery::workflow
