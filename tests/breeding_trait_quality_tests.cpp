#include "test_support.hpp"
#include "auto_cattery/config.hpp"
#include "auto_cattery/breeding/breeding_ranker.hpp"
#include "auto_cattery/breeding/population_selection.hpp"
#include "auto_cattery/snapshot/save_snapshot_adapter.hpp"
#include <algorithm>
#include <filesystem>

namespace autocattery::tests {
void RunCatBlobParserTests();
void RunPairRankerTests();
void RunBreedingRankerTests();
void RunBreedingScorerTests();
void RunClassifierTests();
void RunBalancedMoveOnlyPlannerTests();
void RunConfigBoundaryTests();
}

int main(int argc, char** argv) {
    using namespace autocattery;
    tests::RunCatBlobParserTests();
    tests::RunPairRankerTests();
    tests::RunBreedingRankerTests();
    tests::RunBreedingScorerTests();
    tests::RunClassifierTests();
    tests::RunBalancedMoveOnlyPlannerTests();
    tests::RunConfigBoundaryTests();
    if (argc == 5) {
        const auto config = LoadConfig(argv[3], argv[4]);
        AC_CHECK(static_cast<bool>(config));
        if (!config) { std::cerr << config.message << '\n'; return 1; }
        snapshot::SaveSnapshotAdapter adapter(argv[2], argv[1]);
        const auto house = adapter.CaptureHouseSnapshot(1);
        AC_CHECK(static_cast<bool>(house));
        if (!house) { std::cerr << house.message << '\n'; return 1; }
        const auto& c = config.value;
        const bool assisted = c.room_planning.offspring_all_seven_assist && c.mod_enabled &&
            !c.safe_mode && !c.force_read_only && !c.execution_safety.read_only_mode;
        const auto ranking = breeding::RankBreedingCats(house.value, c.breeding_scoring,
            c.room_planning.avoid_inbreeding_pairs, assisted);
        AC_CHECK(ranking && ranking.value.recommended_pair);
        if (ranking && ranking.value.recommended_pair) {
            const auto& pair = *ranking.value.recommended_pair;
            AC_CHECK(pair.offspring_inbreeding_coefficient == 0.0);
            std::cout << "save day=" << house.value.game_day.value_or(-1)
                << " cats=" << house.value.cats.size() << " assisted=" << assisted
                << " pair=" << pair.cat_a_id << ',' << pair.cat_b_id
                << " trait_score=" << pair.trait_score << '\n';
            for (const auto id : {pair.cat_a_id, pair.cat_b_id}) {
                const auto& cat = *std::ranges::find(house.value.cats, id, &snapshot::CatSnapshot::id);
                std::cout << "parent " << id << " slots=";
                for (const auto& slot : cat.raw_ability_slots) std::cout << slot << ',';
                std::cout << '\n';
            }
        }
        std::vector<protection::ProtectionCatOption> options;
        for (const auto& cat : house.value.cats) options.push_back({.cat_id=cat.id});
        const auto selection = breeding::SelectPopulation(house.value, options, c);
        AC_CHECK(static_cast<bool>(selection));
        if (selection && ranking && ranking.value.recommended_pair) {
            const auto& pair = *ranking.value.recommended_pair;
            AC_CHECK(std::ranges::find(selection.value.retained, pair.cat_a_id) != selection.value.retained.end());
            AC_CHECK(std::ranges::find(selection.value.retained, pair.cat_b_id) != selection.value.retained.end());
        }
    }
    std::cout << "trait quality failures=" << tests::failures << '\n';
    return tests::failures ? 1 : 0;
}
