#include "auto_cattery/breeding/breeding_ranker.hpp"
#include "auto_cattery/snapshot/save_snapshot_adapter.hpp"
#include "auto_cattery/workflow/preview_builder.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <limits>
#include <tuple>

namespace {

const char* Stage(autocattery::breeding::BreedingStage stage) {
    using enum autocattery::breeding::BreedingStage;
    switch (stage) {
        case Foundation: return "foundation";
        case BaseAllSeven: return "base-all-seven";
        case StableAllSeven: return "stable-all-seven";
    }
    return "unknown";
}

const autocattery::snapshot::CatSnapshot* FindCat(
    const autocattery::snapshot::HouseSnapshot& house,
    autocattery::snapshot::CatId id) {
    const auto found = std::ranges::find(
        house.cats, id, &autocattery::snapshot::CatSnapshot::id);
    return found == house.cats.end() ? nullptr : &*found;
}

const autocattery::snapshot::RoomSnapshot* BestBreedingRoom(
    const autocattery::snapshot::HouseSnapshot& house,
    bool stable) {
    const autocattery::snapshot::RoomSnapshot* best{};
    for (const auto& room : house.rooms) {
        if (!room.attributes) {
            continue;
        }
        const auto key = [&](const auto& value) {
            const auto& a = *value.attributes;
            return stable
                ? std::tuple{-a.stimulation, -a.mutation, value.id}
                : std::tuple{-a.stimulation, -a.comfort, value.id};
        };
        if (best == nullptr || key(room) < key(*best)) {
            best = &room;
        }
    }
    return best;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    const std::filesystem::path save = argc > 1 ? argv[1] : L"";
    autocattery::Config config;
    if (argc > 2) {
        const std::filesystem::path config_root = argv[2];
        const auto loaded = autocattery::LoadConfig(
            config_root / L"default_config.json", config_root / L"user_config.json");
        if (!loaded) {
            std::cerr << "config failed: " << loaded.message << '\n';
            return 3;
        }
        config = loaded.value;
    }
    const auto game_root = std::filesystem::current_path().parent_path();
    autocattery::snapshot::SaveSnapshotAdapter adapter(save, game_root);
    const auto captured = adapter.CaptureHouseSnapshot(1);
    if (!captured) {
        std::cerr << "snapshot failed: " << captured.message << '\n';
        return 1;
    }
    const auto& house = captured.value;
    std::size_t straight{};
    std::size_t bisexual{};
    std::size_t gay{};
    std::size_t genetic_values{};
    std::size_t mutations{};
    std::size_t birth_defects{};
    auto genetic_min = std::numeric_limits<std::int32_t>::max();
    auto genetic_max = std::numeric_limits<std::int32_t>::min();
    for (const auto& cat : house.cats) {
        for (const auto& trait : cat.visual_traits) {
            if (trait.kind ==
                autocattery::snapshot::VisualTraitKind::BirthDefect) {
                ++birth_defects;
            } else {
                ++mutations;
            }
        }
        for (const auto& value : cat.genetic_stats.values) {
            if (value) {
                ++genetic_values;
                genetic_min = std::min(genetic_min, *value);
                genetic_max = std::max(genetic_max, *value);
            }
        }
        switch (cat.sexuality) {
            case autocattery::snapshot::CatSexuality::Straight: ++straight; break;
            case autocattery::snapshot::CatSexuality::Bisexual: ++bisexual; break;
            case autocattery::snapshot::CatSexuality::Gay: ++gay; break;
            case autocattery::snapshot::CatSexuality::Unknown: break;
        }
    }
    const auto ranking = autocattery::breeding::RankBreedingCats(
        house, config.breeding_scoring, config.room_planning.avoid_inbreeding_pairs);
    if (!ranking) {
        std::cerr << "ranking failed: " << ranking.message << '\n';
        return 2;
    }
    std::cout
        << "cats=" << house.cats.size()
        << " base_stats=" << house.capabilities.read_genetic_stats
        << " genetic_values=" << genetic_values
        << " genetic_range="
        << (genetic_values ? std::to_string(genetic_min) : "unknown")
        << ".."
        << (genetic_values ? std::to_string(genetic_max) : "unknown")
        << " sexuality=" << house.capabilities.read_sexuality
        << " pedigree=" << house.capabilities.read_relationships
        << " visual_traits=" << house.capabilities.read_visual_traits
        << " mutations=" << mutations
        << " birth_defects=" << birth_defects
        << " pair_coi=" << house.pedigree_pair_coefficients.size()
        << " straight=" << straight
        << " bisexual=" << bisexual
        << " gay=" << gay
        << " stage=" << Stage(ranking.value.stage);
    if (ranking.value.recommended_pair) {
        const auto& pair = *ranking.value.recommended_pair;
        const auto* a = FindCat(house, pair.cat_a_id);
        const auto* b = FindCat(house, pair.cat_b_id);
        const auto* room = BestBreedingRoom(
            house,
            ranking.value.stage ==
                autocattery::breeding::BreedingStage::StableAllSeven);
        std::cout
            << " pair=" << pair.cat_a_id << ',' << pair.cat_b_id
            << " names=" << (a ? a->display_name : "?") << '+'
            << (b ? b->display_name : "?")
            << " coverage=" << pair.covered_seven_stats
            << " stable=" << pair.jointly_stable_seven_stats
            << " coi=" << *pair.offspring_inbreeding_coefficient
            << " trait_score=" << pair.trait_score
            << " target=" << (room ? room->id : "unavailable");
    }
    autocattery::workflow::WorkflowStateMachine state;
    if (state.BeginPreview()) {
        const auto preview = autocattery::workflow::PreviewBuilder(adapter, config).Build(
            2, autocattery::workflow::WorkflowCapability::MoveOnly, state);
        if (preview) {
            std::cout << " preview_moves="
                      << preview.value.room_plan.moves.size();
            for (const auto& error : preview.value.room_plan.validation_errors) {
                std::cout << " preview_error=" << error;
            }
            if (ranking.value.recommended_pair) {
                auto room_for = [&](autocattery::snapshot::CatId id) {
                    auto room = FindCat(preview.value.snapshot, id)->room_id
                        .value_or("Outside");
                    for (const auto& move : preview.value.room_plan.moves) {
                        if (move.cat_id == id) {
                            room = move.to_room;
                        }
                    }
                    return room;
                };
                const auto& pair = *ranking.value.recommended_pair;
                std::cout << " preview_pair_rooms="
                          << room_for(pair.cat_a_id) << ','
                          << room_for(pair.cat_b_id);
                std::size_t residents{};
                std::size_t cross_pairs{};
                std::size_t covered_cross_pairs{};
                std::size_t current_residents{};
                std::size_t current_cross_pairs{};
                std::size_t current_covered_cross_pairs{};
                const auto target = room_for(pair.cat_a_id);
                for (const auto& cat : preview.value.snapshot.cats) {
                    residents += room_for(cat.id) == target ? 1U : 0U;
                    current_residents += cat.room_id == target ? 1U : 0U;
                }
                for (const auto& cross : ranking.value.ranked_pairs) {
                    if (!cross.eligible) {
                        continue;
                    }
                    if (FindCat(house, cross.cat_a_id)->room_id == target &&
                        FindCat(house, cross.cat_b_id)->room_id == target) {
                        ++current_cross_pairs;
                        current_covered_cross_pairs += cross.covered_seven_stats >=
                            pair.covered_seven_stats ? 1U : 0U;
                    }
                    if (room_for(cross.cat_a_id) != target ||
                        room_for(cross.cat_b_id) != target) {
                        continue;
                    }
                    ++cross_pairs;
                    covered_cross_pairs += cross.covered_seven_stats >=
                        pair.covered_seven_stats ? 1U : 0U;
                }
                std::cout << " preview_pool_residents=" << residents
                          << " preview_eligible_cross_pairs=" << cross_pairs
                          << " preview_target_coverage_cross_pairs="
                          << covered_cross_pairs
                          << " current_pool_residents=" << current_residents
                          << " current_eligible_cross_pairs=" << current_cross_pairs
                          << " current_target_coverage_cross_pairs="
                          << current_covered_cross_pairs;
            }
        }
    }
    std::cout << '\n';
    return 0;
}
