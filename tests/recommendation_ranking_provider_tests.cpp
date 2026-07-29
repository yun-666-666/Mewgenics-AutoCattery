#include "auto_cattery/recommendation/ranking_provider.hpp"

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

snapshot::CatSnapshot Candidate(snapshot::CatId id, std::int32_t value) {
    snapshot::CatSnapshot cat;
    cat.id = id;
    cat.life_stage = snapshot::LifeStage::Adult;
    cat.available_for_combat = snapshot::TriState::Yes;
    cat.injured = snapshot::TriState::No;
    cat.raw_ability_slots.resize(10);
    cat.room_id = "verified-candidate-source";
    for (std::size_t index = 0; index < snapshot::kStatCount; ++index) {
        cat.genetic_stats.values[index] = value;
        cat.heredity_bonus.values[index] = 0;
        cat.equipment_bonus.values[index] = 0;
    }
    return cat;
}

class CandidateSourceFake final
    : public recommendation::ICurrentCombatCandidateSource {
public:
    Result<snapshot::HouseSnapshot> CaptureConfirmedCandidates(
        const ui::UiContextSnapshot&) override {
        ++calls;
        return {house};
    }

    int calls{};
    snapshot::HouseSnapshot house;
};

ui::UiContextSnapshot EmbarkContext(std::uint64_t generation = 7) {
    return {
        ui::UiContextKind::EmbarkSelection,
        "fixture-embark",
        generation,
        true,
        false,
        {"fixture"}
    };
}

snapshot::HouseSnapshot CandidateHouse(
    std::size_t count,
    std::uint64_t generation = 7) {
    snapshot::HouseSnapshot house;
    house.snapshot_id = 10;
    house.scene_generation = generation;
    house.capabilities.stable_cat_id = true;
    house.rooms.push_back({.id = "verified-candidate-source"});
    for (std::size_t index = 0; index < count; ++index) {
        const auto id = static_cast<snapshot::CatId>(count - index);
        house.cats.push_back(Candidate(id, 5));
        house.rooms.front().residents.push_back(id);
    }
    return house;
}

}  // namespace

void RunRecommendationRankingProviderTests() {
    CandidateSourceFake source;
    source.house = CandidateHouse(3);
    scoring::CombatScoringConfig config;
    config.recommended_count = 8;
    recommendation::InstantRankingProvider provider(source, config);

    AC_CHECK(provider.ComputationCount() == 0);
    AC_CHECK(source.calls == 0);
    const auto ranked = provider.Recompute(EmbarkContext());
    AC_CHECK(static_cast<bool>(ranked));
    AC_CHECK(ranked.value.candidate_count == 3);
    AC_CHECK(ranked.value.ranking.recommended_cat_ids ==
             std::vector<snapshot::CatId>({1, 2, 3}));
    AC_CHECK(provider.ComputationCount() == 1);

    source.house = CandidateHouse(1);
    source.house.cats.front().available_for_combat =
        snapshot::TriState::Unknown;
    const auto unknown = provider.Recompute(EmbarkContext());
    AC_CHECK(static_cast<bool>(unknown));
    AC_CHECK(unknown.value.ranking.recommended_cat_ids.empty());

    source.house = CandidateHouse(0);
    const auto empty = provider.Recompute(EmbarkContext());
    AC_CHECK(static_cast<bool>(empty));
    AC_CHECK(empty.value.ranking.ranked.empty());

    auto unsafe = EmbarkContext();
    unsafe.kind = ui::UiContextKind::House;
    AC_CHECK(!static_cast<bool>(provider.Recompute(unsafe)));
    AC_CHECK(source.calls == 3);

    source.house = CandidateHouse(100);
    const auto hundred = provider.Recompute(EmbarkContext());
    AC_CHECK(static_cast<bool>(hundred));
    AC_CHECK(hundred.value.ranking.ranked.front().cat_id == 1);
    source.house = CandidateHouse(1'000);
    const auto thousand = provider.Recompute(EmbarkContext());
    AC_CHECK(static_cast<bool>(thousand));
    AC_CHECK(thousand.value.ranking.ranked.front().cat_id == 1);

    recommendation::UnsupportedCombatCandidateSource unsupported;
    recommendation::InstantRankingProvider blocked(unsupported, config);
    AC_CHECK(!static_cast<bool>(blocked.Recompute(EmbarkContext())));
    AC_CHECK(blocked.ComputationCount() == 0);
}

}  // namespace autocattery::tests
