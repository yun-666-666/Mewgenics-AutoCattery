#include "config_json.hpp"

#include <array>

#include "auto_cattery/breeding/breeding_scorer.hpp"
#include "auto_cattery/classification/classifier.hpp"
#include "auto_cattery/scoring/combat_scorer.hpp"
#include "auto_cattery/version.hpp"

namespace autocattery::config_detail {
namespace {

void DecodeWeights(
    const Json& object,
    std::array<double, snapshot::kStatCount>& destination) {
    static constexpr std::array<const char*, snapshot::kStatCount> keys{
        "strength",
        "dexterity",
        "constitution",
        "intelligence",
        "speed",
        "charisma",
        "luck"
    };
    for (std::size_t index = 0; index < keys.size(); ++index) {
        destination[index] = object.at(keys[index]).get<double>();
    }
}

void DecodeOverrides(
    const Json& object,
    std::unordered_map<std::string, double>& destination) {
    destination.clear();
    for (const auto& [key, value] : object.items()) {
        destination.emplace(key, value.get<double>());
    }
}

void DecodeFurnitureTargets(
    const Json& value,
    furniture_planning::FurniturePurposeTargets& result) {
    result.comfort_per_resident =
        value.at("comfort_per_resident").get<double>();
    result.stimulation_per_resident =
        value.at("stimulation_per_resident").get<double>();
    result.health_per_resident =
        value.at("health_per_resident").get<double>();
    result.mutation_per_resident =
        value.at("mutation_per_resident").get<double>();
}

template<class ScoringConfig>
void DecodeCommonScoring(const Json& value, ScoringConfig& result) {
    result.version = value.at("version").get<std::uint32_t>();
    result.minimum_score = value.at("minimum_score").get<double>();
    result.minimum_known_stats = value.at("minimum_known_stats").get<std::size_t>();
    result.require_confirmed_eligibility =
        value.at("require_confirmed_eligibility").get<bool>();
    result.missing_stat_penalty = value.at("missing_stat_penalty").get<double>();
    result.active_ability_default_weight =
        value.at("active_ability_default_weight").get<double>();
    result.passive_default_weight =
        value.at("passive_default_weight").get<double>();
    result.disorder_default_penalty =
        value.at("disorder_default_penalty").get<double>();
    DecodeWeights(value.at("stat_weights"), result.stat_weights);
    DecodeOverrides(
        value.at("active_ability_overrides"),
        result.active_ability_overrides);
    DecodeOverrides(
        value.at("passive_overrides"),
        result.passive_overrides);
    DecodeOverrides(
        value.at("disorder_overrides"),
        result.disorder_overrides);
}

}  // namespace

Result<Config> DecodeConfig(const Json& value) {
    Config result;
    try {
        result.schema_version = value.at("schema_version").get<int>();
        const auto& general = value.at("general");
        result.general.version = general.at("version").get<std::uint32_t>();
        result.general.mod_enabled = general.at("mod_enabled").get<bool>();
        result.general.safe_mode = general.at("safe_mode").get<bool>();
        result.general.log_level = general.at("log_level").get<std::string>();
        result.general.language = general.at("language").get<std::string>();
        result.mod_enabled = result.general.mod_enabled;
        result.safe_mode = result.general.safe_mode;
        result.log_level = result.general.log_level;
        result.language = result.general.language;

        const auto& ui = value.at("ui");
        result.ui.version = ui.at("version").get<std::uint32_t>();
        result.ui.house_button_enabled = ui.at("house_button_enabled").get<bool>();
        result.ui.embark_button_enabled = ui.at("embark_button_enabled").get<bool>();
        result.ui.show_debug_overlay = ui.at("show_debug_overlay").get<bool>();

        const auto& safety = value.at("execution_safety");
        result.execution_safety.version = safety.at("version").get<std::uint32_t>();
        result.execution_safety.require_preview_before_destructive_actions =
            safety.at("require_preview_before_destructive_actions").get<bool>();
        result.execution_safety.create_backup_before_apply =
            safety.at("create_backup_before_apply").get<bool>();
        result.execution_safety.read_only_mode =
            safety.at("read_only_mode").get<bool>();
        result.execution_safety.single_click_execute =
            safety.at("single_click_execute").get<bool>();
        result.execution_safety.abort_on_unknown_game_build =
            safety.at("abort_on_unknown_game_build").get<bool>();
        result.safety = result.execution_safety;

        const auto& execution = value.at("execution");
        result.execution.real_write_adapter_enabled =
            execution.at("real_write_adapter_enabled").get<bool>();
        result.execution.cull_enabled = execution.at("cull_enabled").get<bool>();
        result.execution.require_quiescent_backup =
            execution.at("require_quiescent_backup").get<bool>();
        result.workflow.preview_ttl_seconds = value.at("workflow")
            .at("preview_ttl_seconds").get<std::uint32_t>();

        const auto& combat = value.at("combat_scoring");
        DecodeCommonScoring(combat, result.combat_scoring);
        result.combat_scoring.recommended_count =
            combat.at("recommended_count").get<std::size_t>();
        result.combat_scoring.exclude_kittens =
            combat.at("exclude_kittens").get<bool>();
        result.combat_scoring.exclude_injured =
            combat.at("exclude_injured").get<bool>();
        result.combat_scoring.injury_penalty =
            combat.at("injury_penalty").get<double>();

        const auto& breeding = value.at("breeding_scoring");
        DecodeCommonScoring(breeding, result.breeding_scoring);
        result.breeding_scoring.core_breeders =
            breeding.at("core_breeders").get<std::size_t>();
        result.breeding_scoring.reserve_breeders =
            breeding.at("reserve_breeders").get<std::size_t>();
        result.breeding_scoring.mutation_default_weight =
            breeding.at("mutation_default_weight").get<double>();
        result.breeding_scoring.birth_defect_default_penalty =
            breeding.at("birth_defect_default_penalty").get<double>();
        DecodeOverrides(
            breeding.at("mutation_overrides"),
            result.breeding_scoring.mutation_overrides);
        DecodeOverrides(
            breeding.at("birth_defect_overrides"),
            result.breeding_scoring.birth_defect_overrides);

        const auto& classification = value.at("classification");
        result.classification.version =
            classification.at("version").get<std::uint32_t>();
        result.classification.combat_priority_over_breeding =
            classification.at("combat_priority_over_breeding").get<bool>();
        result.classification.minimum_combat_pool =
            classification.at("minimum_combat_pool").get<std::size_t>();
        result.classification.minimum_breeding_pool =
            classification.at("minimum_breeding_pool").get<std::size_t>();
        result.classification.minimum_general_reserve =
            classification.at("minimum_general_reserve").get<std::size_t>();
        result.classification.never_cull_if_data_confidence_below =
            classification.at("never_cull_if_data_confidence_below").get<double>();

        const auto& protection = value.at("protection");
        result.protection.version = protection.at("version").get<std::uint32_t>();
        result.protection.sidecar_file = protection.at("sidecar_file").get<std::string>();
        result.protection.protect_unknown_native_state =
            protection.at("protect_unknown_native_state").get<bool>();
        result.protection.require_stable_identity_for_sidecar =
            protection.at("require_stable_identity_for_sidecar").get<bool>();

        const auto& planning = value.at("room_planning");
        result.room_planning.version = planning.at("version").get<std::uint32_t>();
        result.room_planning.default_soft_capacity =
            planning.at("default_soft_capacity").get<std::size_t>();
        result.room_planning.allow_soft_overflow =
            planning.at("allow_soft_overflow").get<bool>();
        result.room_planning.max_soft_overflow_per_room =
            planning.at("max_soft_overflow_per_room").get<std::size_t>();
        result.room_planning.never_exceed_known_hard_capacity =
            planning.at("never_exceed_known_hard_capacity").get<bool>();
        result.room_planning.prefer_single_combat_staging_room =
            planning.at("prefer_single_combat_staging_room").get<bool>();
        result.room_planning.keep_breeding_pairs_together =
            planning.at("keep_breeding_pairs_together").get<bool>();
        result.room_planning.avoid_inbreeding_pairs =
            planning.at("avoid_inbreeding_pairs").get<bool>();
        result.room_planning.keep_kittens_separate_when_possible =
            planning.at("keep_kittens_separate_when_possible").get<bool>();
        result.room_planning.allow_partial_plan =
            planning.at("allow_partial_plan").get<bool>();

        const auto& furniture = value.at("furniture_placement");
        result.furniture_placement.version =
            furniture.at("version").get<std::uint32_t>();
        DecodeFurnitureTargets(
            furniture.at("breeding"),
            result.furniture_placement.breeding);
        DecodeFurnitureTargets(
            furniture.at("kitten_recovery"),
            result.furniture_placement.kitten_recovery);
        DecodeFurnitureTargets(
            furniture.at("combat"),
            result.furniture_placement.combat);
        DecodeFurnitureTargets(
            furniture.at("mutation"),
            result.furniture_placement.mutation);
        DecodeFurnitureTargets(
            furniture.at("general"),
            result.furniture_placement.general);
        result.furniture_placement.minimum_furnishing_coverage_percent =
            furniture.at("minimum_furnishing_coverage_percent")
                .get<std::size_t>();
        // Keep accepting the legacy key, but do not let an older saved false
        // value turn the organizer back into a sparse "targets are enough"
        // mode. Maximum safe room filling is the fixed product behavior.
        (void)furniture.at("fill_remaining_capacity").get<bool>();
        result.furniture_placement.fill_remaining_capacity = true;

        const auto& marker = value.at("recommendation_marker");
        result.recommendation_marker.version =
            marker.at("version").get<std::uint32_t>();
        result.recommendation_marker.recommended_count =
            marker.at("recommended_count").get<std::size_t>();
        result.recommendation_marker.show_score = marker.at("show_score").get<bool>();
        result.recommendation_marker.show_rank = marker.at("show_rank").get<bool>();
        result.recommendation_marker.pulse_top_n =
            marker.at("pulse_top_n").get<std::size_t>();
        result.recommendation_marker.auto_clear_on_scene_exit =
            marker.at("auto_clear_on_scene_exit").get<bool>();
        result.recommendation_marker.recompute_if_stale =
            marker.at("recompute_if_stale").get<bool>();
        result.recommendation_marker.never_auto_select = true;

        const auto& diagnostics = value.at("diagnostics");
        result.diagnostics.version = diagnostics.at("version").get<std::uint32_t>();
        result.diagnostics.show_debug_overlay =
            diagnostics.at("show_debug_overlay").get<bool>();
        result.diagnostics.export_scene_summary_enabled =
            diagnostics.at("export_scene_summary_enabled").get<bool>();
        result.diagnostics.collect_cat_data =
            diagnostics.at("collect_cat_data").get<bool>();

        const auto& level_up = value.at("level_up");
        result.level_up.version = level_up.at("version").get<std::uint32_t>();
        result.level_up.reroll_count =
            level_up.at("reroll_count").get<std::size_t>();
    } catch (const Json::exception& exception) {
        return {{}, ErrorCode::ConfigInvalid, exception.what()};
    }

    const auto combat = scoring::Validate(result.combat_scoring);
    if (!combat) {
        return {{}, combat.code, "combat_scoring: " + combat.message};
    }
    const auto breeding = breeding::Validate(result.breeding_scoring);
    if (!breeding) {
        return {{}, breeding.code, "breeding_scoring: " + breeding.message};
    }
    const auto classification = classification::Validate(result.classification);
    if (!classification) {
        return {{}, classification.code, "classification: " + classification.message};
    }
    result.force_read_only = result.schema_version > kSupportedConfigSchema;
    if (result.force_read_only) {
        result.execution_safety.read_only_mode = true;
        result.safety = result.execution_safety;
    }
    return {std::move(result)};
}

}  // namespace autocattery::config_detail
