#include "auto_cattery/breeding/population_selection.hpp"
#include <algorithm>
#include <tuple>
#include <unordered_set>
#include "auto_cattery/breeding/pair_ranker.hpp"
#include "auto_cattery/breeding/lineage_selection.hpp"
#include "auto_cattery/scoring/combat_ranker.hpp"
#include "population_lineage_reserve.hpp"
#include "pair_trait_scorer.hpp"

namespace autocattery::breeding {
Result<PopulationSelection> SelectPopulation(
    const snapshot::HouseSnapshot& house,
    std::span<const protection::ProtectionCatOption> options,
    const Config& config) {
    const auto limit = config.room_planning.population_limit;
    if (limit < 4 || limit > 1000)
        return {{}, ErrorCode::SnapshotInvalid, "猫数量上限须为4至1000"};
    const auto combat = scoring::RankCombatCats(house, config.combat_scoring);
    if (!combat) return {{}, combat.code, combat.message};
    const bool assisted = config.room_planning.offspring_all_seven_assist &&
        config.mod_enabled && !config.safe_mode && !config.force_read_only &&
        !config.execution_safety.read_only_mode;
    const auto pairs = RankBreedingPairs(house, config.breeding_scoring, assisted);
    if (!pairs) return {{}, pairs.code, pairs.message};
    PopulationSelection result;
    std::unordered_set<snapshot::CatId> keep;
    for (const auto& cat : house.cats) {
        const auto option = std::ranges::find(options, cat.id,
            &protection::ProtectionCatOption::cat_id);
        // Unknown identity/state and player protections are never cull candidates.
        if (option == options.end() || option->fixed_room ||
            (option->level && *option->level != protection::ProtectionLevel::None) ||
            cat.in_adventure_box || cat.life_stage == snapshot::LifeStage::Unknown ||
            std::ranges::any_of(cat.genetic_stats.values, [](auto value) { return !value; }))
            keep.insert(cat.id);
    }
    result.protected_count = keep.size();
    std::unordered_set<snapshot::CatId> paired;
    if (config.room_planning.avoid_inbreeding_pairs) {
        const auto families = IndependentBreedingFamilies(pairs.value.ranked, house);
        auto candidate = keep;
        candidate.insert(families.begin(), families.end());
        if (candidate.size() <= limit) {
            keep = std::move(candidate);
            paired.insert(families.begin(), families.end());
        }
    }
    for (const auto& pair : pairs.value.ranked) {
        if (paired.size() >= 4) break;
        if (!pair.eligible || paired.contains(pair.cat_a_id) || paired.contains(pair.cat_b_id) ||
            (config.room_planning.avoid_inbreeding_pairs &&
                pair.offspring_inbreeding_coefficient.value_or(1.0) > 0.0)) continue;
        const auto needed = static_cast<std::size_t>(!keep.contains(pair.cat_a_id)) +
            static_cast<std::size_t>(!keep.contains(pair.cat_b_id));
        if (keep.size() + needed > limit) continue;
        keep.insert(pair.cat_a_id);
        keep.insert(pair.cat_b_id);
        paired.insert(pair.cat_a_id);
        paired.insert(pair.cat_b_id);
    }
    ReserveUnrepresentedFounders(house, paired, limit, keep);
    for (const auto id : combat.value.recommended_cat_ids) {
        if (keep.size() >= limit) break;
        keep.insert(id);
    }
    std::vector<const snapshot::CatSnapshot*> ranked;
    for (const auto& cat : house.cats) ranked.push_back(&cat);
    const auto key = [&](const snapshot::CatSnapshot& cat) {
        int sum{}, sevens{};
        for (const auto value : cat.genetic_stats.values) {
            sum += value.value_or(0);
            sevens += value == 7;
        }
        const auto score = std::ranges::find(combat.value.ranked, cat.id,
            &scoring::CombatScoreResult::cat_id);
        const auto traits = detail::StablePairTraitScore(house, cat, cat, config.breeding_scoring);
        return std::tuple{cat.life_stage != snapshot::LifeStage::Dead,
            assisted ? traits : 0.0,
            sevens == 7, sum, sevens,
            traits,
            cat.available_for_breeding == snapshot::TriState::Yes,
            score == combat.value.ranked.end() ? 0.0 : score->score,
            -cat.age_days.value_or(0)};
    };
    std::ranges::sort(ranked, [&](const auto* a, const auto* b) {
        return key(*a) != key(*b) ? key(*a) > key(*b) : a->id < b->id;
    });
    for (const auto* cat : ranked) {
        if (keep.size() < limit) keep.insert(cat->id);
        (keep.contains(cat->id) ? result.retained : result.surplus).push_back(cat->id);
    }
    std::ranges::sort(result.retained);
    std::ranges::sort(result.surplus);
    result.limit_reached = result.retained.size() <= limit;
    return {std::move(result)};
}
}
