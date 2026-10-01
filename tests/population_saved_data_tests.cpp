#include "auto_cattery/breeding/breeding_ranker.hpp"
#include "auto_cattery/breeding/population_selection.hpp"
#include "auto_cattery/protection/editor_model.hpp"
#include "auto_cattery/snapshot/save_snapshot_adapter.hpp"
#include "test_support.hpp"

#include <algorithm>
#include <iostream>

int wmain(int argc, wchar_t** argv) {
    using namespace autocattery;
    snapshot::HouseSnapshot house;
    house.capabilities.read_genetic_stats = house.capabilities.read_sexuality =
        house.capabilities.read_relationships = true;
    std::vector<protection::ProtectionCatOption> options;
    for (int id = 1; id <= 8; ++id) {
        snapshot::CatSnapshot cat;
        cat.id = id;
        cat.life_stage = snapshot::LifeStage::Adult;
        cat.genetic_stats.values.fill(id <= 4 ? 7 : 2);
        cat.available_for_breeding = snapshot::TriState::Yes;
        cat.available_for_combat = snapshot::TriState::No;
        cat.sex = id % 2 ? snapshot::CatSex::Male : snapshot::CatSex::Female;
        cat.sexuality_coefficient = 0;
        cat.libido = snapshot::CatLibido::Normal;
        house.cats.push_back(cat);
        options.push_back({.cat_id = id});
    }
    // Only one unrelated pair: independent-family selection cannot hide a
    // related top-scoring pair slipping through the subsequent pair loop.
    house.pedigree_pair_coefficients = {{1, 2, 0.25}, {3, 4, 0.25}, {5, 6, 0}};
    Config config;
    config.room_planning.population_limit = 4;
    auto selected = breeding::SelectPopulation(house, options, config);
    AC_CHECK(static_cast<bool>(selected));
    AC_CHECK(std::ranges::find(selected.value.retained, 5) != selected.value.retained.end());
    AC_CHECK(std::ranges::find(selected.value.retained, 6) != selected.value.retained.end());
    config.room_planning.avoid_inbreeding_pairs = false;
    selected = breeding::SelectPopulation(house, options, config);
    AC_CHECK((selected.value.retained == std::vector<snapshot::CatId>{1, 2, 3, 4}));

    if (argc > 1) {
        snapshot::SaveSnapshotAdapter adapter(argv[1], argc > 2 ? argv[2] : L"");
        const auto captured = adapter.CaptureHouseSnapshot(1);
        AC_CHECK(static_cast<bool>(captured));
        if (!captured) return 1;
        const auto& saved = captured.value;
        AC_CHECK(saved.capabilities.read_genetic_stats);
        AC_CHECK(saved.capabilities.read_sexuality);
        AC_CHECK(saved.capabilities.read_relationships);
        config = Config{};
        if (argc > 3) {
            const std::filesystem::path config_root = argv[3];
            const auto loaded = LoadConfig(config_root / "default_config.json",
                config_root / "user_config.json");
            AC_CHECK(static_cast<bool>(loaded));
            if (!loaded) return 1;
            config = loaded.value;
            protection::ProtectionEditorModel protection(
                config_root / config.protection.sidecar_file,
                std::vector<snapshot::HouseSnapshot>{saved});
            AC_CHECK(static_cast<bool>(protection.Reload()));
            options.assign(protection.cats().begin(), protection.cats().end());
        } else {
            options.clear();
            for (const auto& cat : saved.cats) options.push_back({.cat_id = cat.id});
        }
        const auto breeders = breeding::RankBreedingCats(saved, config.breeding_scoring, true);
        AC_CHECK(static_cast<bool>(breeders));
        AC_CHECK(breeders.value.recommended_pair.has_value());
        if (breeders.value.recommended_pair)
            AC_CHECK(breeders.value.recommended_pair->offspring_inbreeding_coefficient == 0.0);
        selected = breeding::SelectPopulation(saved, options, config);
        AC_CHECK(static_cast<bool>(selected));
        AC_CHECK(selected.value.limit_reached);
        AC_CHECK(selected.value.retained.size() ==
            std::min(saved.cats.size(), config.room_planning.population_limit));
        AC_CHECK(selected.value.retained.size() + selected.value.surplus.size() == saved.cats.size());
        std::cout << "saved_cats=" << saved.cats.size()
                  << " retained=" << selected.value.retained.size()
                  << " surplus=" << selected.value.surplus.size()
                  << " protected=" << selected.value.protected_count
                  << " ranked_pairs=" << breeders.value.ranked_pairs.size() << '\n';
    }
    return tests::failures ? 1 : 0;
}
