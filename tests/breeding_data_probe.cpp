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
                ? std::tuple{-a.comfort, -a.stimulation, value.id}
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
    auto genetic_min = std::numeric_limits<std::int32_t>::max();
    auto genetic_max = std::numeric_limits<std::int32_t>::min();
    for (const auto& cat : house.cats) {
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
        house, autocattery::breeding::BreedingScoringConfig{});
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
            << " target=" << (room ? room->id : "unavailable");
    }
    autocattery::workflow::WorkflowStateMachine state;
    if (state.BeginPreview()) {
        const auto preview = autocattery::workflow::PreviewBuilder(adapter).Build(
            2, autocattery::workflow::WorkflowCapability::MoveOnly, state);
        if (preview) {
            std::cout << " preview_moves="
                      << preview.value.room_plan.moves.size();
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
            }
        }
    }
    std::cout << '\n';
    return 0;
}
