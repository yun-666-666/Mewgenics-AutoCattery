#include "auto_cattery/snapshot/save_snapshot_adapter.hpp"
#include "auto_cattery/breeding/breeding_ranker.hpp"
#include "auto_cattery/classification/classifier.hpp"
#include "auto_cattery/classification/protection_adapter.hpp"
#include "auto_cattery/room_planning/capability_adapter.hpp"
#include "auto_cattery/room_planning/planner.hpp"
#include "auto_cattery/scoring/combat_ranker.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <vector>

int wmain(int argument_count, wchar_t** arguments) {
    const std::filesystem::path configured_save =
        argument_count > 1 ? arguments[1] : L"";
    autocattery::snapshot::SaveSnapshotAdapter adapter(configured_save);
    const auto result = adapter.CaptureHouseSnapshot(1);
    if (!result) {
        std::cerr << "snapshot failed: " << result.message << '\n';
        return 1;
    }

    const auto& snapshot = result.value;
    const auto assigned = std::count_if(
        snapshot.cats.begin(),
        snapshot.cats.end(),
        [](const auto& cat) { return cat.room_id.has_value(); });
    const auto adventure = std::count_if(
        snapshot.cats.begin(),
        snapshot.cats.end(),
        [](const auto& cat) { return cat.in_adventure_box; });
    const auto combat_available = std::count_if(
        snapshot.cats.begin(),
        snapshot.cats.end(),
        [](const auto& cat) {
            return cat.available_for_combat ==
                autocattery::snapshot::TriState::Yes;
        });
    const auto combat_spent = std::count_if(
        snapshot.cats.begin(),
        snapshot.cats.end(),
        [](const auto& cat) {
            return cat.available_for_combat ==
                autocattery::snapshot::TriState::No;
        });
    const auto dead = std::count_if(
        snapshot.cats.begin(),
        snapshot.cats.end(),
        [](const auto& cat) {
            return cat.life_stage ==
                autocattery::snapshot::LifeStage::Dead;
        });
    const auto validation = autocattery::snapshot::Validate(snapshot);
    const auto repeated = adapter.CaptureHouseSnapshot(1);
    std::vector<autocattery::snapshot::CatId> first_ids;
    std::vector<autocattery::snapshot::CatId> repeated_ids;
    first_ids.reserve(snapshot.cats.size());
    if (repeated) {
        repeated_ids.reserve(repeated.value.cats.size());
    }
    for (const auto& cat : snapshot.cats) {
        first_ids.push_back(cat.id);
    }
    if (repeated) {
        for (const auto& cat : repeated.value.cats) {
            repeated_ids.push_back(cat.id);
        }
    }
    const bool stable_ids =
        repeated && first_ids == repeated_ids;
    autocattery::scoring::CombatScoringConfig scoring_config;
    const auto ranking =
        autocattery::scoring::RankCombatCats(snapshot, scoring_config);
    const auto repeated_ranking = repeated
        ? autocattery::scoring::RankCombatCats(
            repeated.value,
            scoring_config)
        : autocattery::Result<autocattery::scoring::CombatRanking>{
            {},
            autocattery::ErrorCode::SnapshotInvalid,
            "repeated snapshot unavailable"
        };
    std::vector<autocattery::snapshot::CatId> ranked_ids;
    std::vector<autocattery::snapshot::CatId> repeated_ranked_ids;
    if (ranking) {
        for (const auto& score : ranking.value.ranked) {
            ranked_ids.push_back(score.cat_id);
        }
    }
    if (repeated_ranking) {
        for (const auto& score : repeated_ranking.value.ranked) {
            repeated_ranked_ids.push_back(score.cat_id);
        }
    }
    const bool stable_ranking =
        ranking && repeated_ranking && ranked_ids == repeated_ranked_ids;
    autocattery::breeding::BreedingScoringConfig breeding_config;
    const auto breeding =
        autocattery::breeding::RankBreedingCats(snapshot, breeding_config);
    const auto repeated_breeding = repeated
        ? autocattery::breeding::RankBreedingCats(
            repeated.value,
            breeding_config)
        : autocattery::Result<
            autocattery::breeding::BreedingRanking>{
                {},
                autocattery::ErrorCode::SnapshotInvalid,
                "repeated snapshot unavailable"
            };
    autocattery::protection::ProtectionSidecar unavailable_sidecar;
    unavailable_sidecar.status =
        autocattery::protection::SidecarLoadStatus::Missing;
    unavailable_sidecar.destructive_actions_blocked = true;
    const auto protection_facts =
        autocattery::classification::BuildCullSafetyFacts(
            snapshot,
            {},
            unavailable_sidecar,
            {});
    const auto repeated_protection_facts = repeated
        ? autocattery::classification::BuildCullSafetyFacts(
            repeated.value,
            {},
            unavailable_sidecar,
            {})
        : autocattery::classification::CullSafetyFactsByCat{};
    std::vector<autocattery::protection::ProtectionDecision>
        protection_decisions;
    std::vector<autocattery::protection::ProtectionDecision>
        repeated_protection_decisions;
    for (const auto& cat : snapshot.cats) {
        protection_decisions.push_back(
            *protection_facts.at(cat.id).policy_decision);
    }
    if (repeated) {
        for (const auto& cat : repeated.value.cats) {
            repeated_protection_decisions.push_back(
                *repeated_protection_facts.at(cat.id).policy_decision);
        }
    }
    const auto protection_digest =
        autocattery::protection::BuildDigest(protection_decisions);
    const auto repeated_protection_digest =
        autocattery::protection::BuildDigest(
            repeated_protection_decisions);
    const bool stable_protection =
        repeated && protection_digest == repeated_protection_digest;
    const auto classification = ranking && breeding
        ? autocattery::classification::ClassifyCats(
            snapshot,
            ranking.value,
            breeding.value,
            breeding_config,
            autocattery::classification::ClassificationConfig{},
            protection_facts)
        : autocattery::Result<
            autocattery::classification::ClassificationPlan>{
                {},
                autocattery::ErrorCode::SnapshotInvalid,
                "ranking unavailable"
            };
    const auto repeated_classification =
        repeated && repeated_ranking && repeated_breeding
        ? autocattery::classification::ClassifyCats(
            repeated.value,
            repeated_ranking.value,
            repeated_breeding.value,
            breeding_config,
            autocattery::classification::ClassificationConfig{},
            repeated_protection_facts)
        : autocattery::Result<
            autocattery::classification::ClassificationPlan>{
                {},
                autocattery::ErrorCode::SnapshotInvalid,
                "repeated ranking unavailable"
            };
    const auto capabilities =
        autocattery::room_planning::BuildConservativeRoomCapabilities(
            snapshot);
    const auto repeated_capabilities = repeated
        ? autocattery::room_planning::BuildConservativeRoomCapabilities(
            repeated.value)
        : std::vector<autocattery::room_planning::RoomCapability>{};
    const auto room_plan = classification
        ? autocattery::room_planning::PlanRooms({
            snapshot,
            classification.value,
            protection_decisions,
            capabilities,
            protection_digest,
            protection_digest
        }, {})
        : autocattery::room_planning::RoomPlan{};
    const auto repeated_room_plan =
        repeated && repeated_classification
        ? autocattery::room_planning::PlanRooms({
            repeated.value,
            repeated_classification.value,
            repeated_protection_decisions,
            repeated_capabilities,
            repeated_protection_digest,
            repeated_protection_digest
        }, {})
        : autocattery::room_planning::RoomPlan{};
    const bool stable_room_plan =
        classification && repeated_classification &&
        room_plan.algorithm_version ==
            repeated_room_plan.algorithm_version &&
        room_plan.moves == repeated_room_plan.moves &&
        room_plan.unplaced_cats == repeated_room_plan.unplaced_cats &&
        room_plan.capacity_relief_suggestions ==
            repeated_room_plan.capacity_relief_suggestions &&
        room_plan.limitations == repeated_room_plan.limitations &&
        room_plan.warnings == repeated_room_plan.warnings &&
        room_plan.validation_errors ==
            repeated_room_plan.validation_errors &&
        room_plan.minimum_capacity_relief_required ==
            repeated_room_plan.minimum_capacity_relief_required &&
        room_plan.disposition == repeated_room_plan.disposition;
    std::cout
        << "house_cats=" << snapshot.cats.size()
        << " rooms=" << snapshot.rooms.size()
        << " assigned=" << assigned
        << " adventure=" << adventure
        << " combat_available=" << combat_available
        << " combat_spent=" << combat_spent
        << " dead=" << dead
        << " day=";
    if (snapshot.game_day) {
        std::cout << *snapshot.game_day;
    } else {
        std::cout << "unavailable";
    }
    std::cout
        << " warnings=" << validation.WarningCount()
        << " errors=" << validation.ErrorCount()
        << " stable_ids=" << (stable_ids ? 1 : 0)
        << " ranked=" << (ranking ? ranking.value.ranked.size() : 0)
        << " recommended="
        << (ranking ? ranking.value.recommended_cat_ids.size() : 0)
        << " stable_ranking=" << (stable_ranking ? 1 : 0)
        << " breeding_ranked="
        << (breeding ? breeding.value.ranked.size() : 0)
        << " protected=" << protection_digest.protected_count
        << " stable_protection=" << (stable_protection ? 1 : 0)
        << " preview_culls="
        << (classification
            ? classification.value.quality_cull_candidates.size()
            : 0)
        << " executable_culls="
        << (classification
            ? std::count_if(
                classification.value.decisions.begin(),
                classification.value.decisions.end(),
                [](const auto& decision) {
                    return decision.destructive_action_allowed;
                })
            : 0)
        << " stable_room_plan=" << (stable_room_plan ? 1 : 0)
        << " planned_moves=" << room_plan.moves.size()
        << " executable_moves="
        << std::count_if(
            room_plan.moves.begin(),
            room_plan.moves.end(),
            [](const auto& move) { return move.executable; })
        << " room_validation_errors="
        << room_plan.validation_errors.size()
        << '\n';
    return validation.Valid() && stable_ids && stable_ranking &&
        stable_protection && stable_room_plan &&
        breeding && classification &&
        classification.value.quality_cull_candidates.empty() &&
        room_plan.validation_errors.empty() &&
        room_plan.moves.empty() &&
        !room_plan.move_execution_allowed &&
        !room_plan.cull_execution_allowed ? 0 : 2;
}
