#include "auto_cattery/workflow/preview_builder.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <utility>

#include "test_support.hpp"
#include "workflow_test_fixture.hpp"

namespace autocattery::tests {
namespace {

using FinalRoomMap =
    std::unordered_map<snapshot::CatId, snapshot::RoomId>;

FinalRoomMap FinalRooms(const workflow::PreviewBundle& bundle) {
  FinalRoomMap result;
  for (const auto& cat : bundle.snapshot.cats) {
    result.emplace(cat.id, cat.room_id.value_or("Outside"));
  }
  for (const auto& move : bundle.room_plan.moves) {
    result.at(move.cat_id) = move.to_room;
  }
  return result;
}

void ApplyRooms(
    snapshot::HouseSnapshot& house,
    const FinalRoomMap& rooms) {
  for (auto& room : house.rooms) {
    room.residents.clear();
  }
  for (auto& cat : house.cats) {
    cat.room_id = rooms.at(cat.id);
    const auto room = std::ranges::find_if(
        house.rooms,
        [&](const auto& candidate) {
          return candidate.id == *cat.room_id;
        });
    AC_CHECK(room != house.rooms.end());
    if (room != house.rooms.end()) {
      room->residents.push_back(cat.id);
    }
  }
}

snapshot::RoomSnapshot& Room(
    snapshot::HouseSnapshot& house,
    const snapshot::RoomId& room_id) {
  return *std::ranges::find_if(
      house.rooms,
      [&](const auto& room) { return room.id == room_id; });
}

void ConfigureRoomPurposes(snapshot::HouseSnapshot& house) {
  house.capabilities.read_room_attributes = true;
  Room(house, "Attic").attributes =
      snapshot::RoomAttributes{.comfort = 30, .stimulation = 40};
  Room(house, "Floor1_Small").attributes =
      snapshot::RoomAttributes{.comfort = 20, .stimulation = 20};
  Room(house, "Floor1_Large").attributes =
      snapshot::RoomAttributes{.comfort = 0, .stimulation = 0};
  if (std::ranges::any_of(
          house.rooms,
          [](const auto& room) { return room.id == "Floor2_Large"; })) {
    Room(house, "Floor2_Large").attributes =
        snapshot::RoomAttributes{.comfort = -5, .stimulation = 10};
  }
}

void ConfigureBreedingPair(
    snapshot::HouseSnapshot& house,
    snapshot::CatId female_id,
    snapshot::CatId male_id) {
  house.capabilities.read_sexuality = true;
  house.capabilities.read_relationships = true;
  for (auto& cat : house.cats) {
    cat.sexuality = snapshot::CatSexuality::Straight;
    cat.sexuality_coefficient = 0.0;
  }
  house.pedigree_pair_coefficients = {{female_id, male_id, 0.0}};
}

std::pair<std::size_t, std::size_t> SexCounts(
    const workflow::PreviewBundle& bundle,
    const FinalRoomMap& rooms,
    const snapshot::RoomId& room_id) {
  std::size_t female{};
  std::size_t male{};
  for (const auto& cat : bundle.snapshot.cats) {
    if (rooms.at(cat.id) != room_id) {
      continue;
    }
    female += cat.sex == snapshot::CatSex::Female ? 1U : 0U;
    male += cat.sex == snapshot::CatSex::Male ? 1U : 0U;
  }
  return {female, male};
}

}  // namespace

void RunBalancedMoveOnlyPlannerTests() {
  WorkflowReadFake reader;
  reader.house = WorkflowHouse(12);
  reader.house.rooms.front().id = "Floor1_Large";
  reader.house.rooms.push_back({.id = "Attic"});
  reader.house.rooms.push_back({.id = "Floor1_Small"});
  reader.house.rooms.push_back({.id = "Floor2_Large"});
  for (auto &cat : reader.house.cats) {
    cat.room_id = "Floor1_Large";
    cat.sex = cat.id <= 2
                  ? snapshot::CatSex::Female
                  : snapshot::CatSex::Male;
  }

  workflow::WorkflowStateMachine state;
  AC_CHECK(state.BeginPreview());
  const auto built = workflow::PreviewBuilder(reader).Build(
      31, workflow::WorkflowCapability::MoveOnly, state);
  AC_CHECK(static_cast<bool>(built));
  AC_CHECK(built.value.room_plan.moves.size() == 9);

  std::unordered_map<snapshot::CatId, snapshot::RoomId> final_room;
  for (const auto &cat : built.value.snapshot.cats) {
    final_room.emplace(cat.id, *cat.room_id);
  }
  for (const auto &move : built.value.room_plan.moves) {
    final_room.at(move.cat_id) = move.to_room;
  }
  std::unordered_map<snapshot::RoomId, std::size_t> counts;
  for (const auto &cat : built.value.snapshot.cats) {
    const auto &room = final_room.at(cat.id);
    ++counts[room];
  }
  AC_CHECK(counts.size() == 4);
  AC_CHECK(std::ranges::all_of(
      counts,
      [](const auto &entry) { return entry.second == 3; }));
  AC_CHECK(std::ranges::none_of(
      built.value.room_plan.moves,
      [](const auto &move) {
        return move.reason == "sex-balance" ||
            move.reason == "sex-balance-and-potential-room";
      }));

  WorkflowReadFake skewed_sexes;
  skewed_sexes.house = WorkflowHouse(85);
  skewed_sexes.house.rooms.front().id = "Floor1_Large";
  skewed_sexes.house.rooms.push_back({.id = "Attic"});
  skewed_sexes.house.rooms.push_back({.id = "Floor1_Small"});
  skewed_sexes.house.rooms.push_back({.id = "Floor2_Large"});
  for (auto &cat : skewed_sexes.house.cats) {
    cat.room_id = "Floor1_Large";
    cat.sex = cat.id <= 8
                  ? snapshot::CatSex::Female
                  : snapshot::CatSex::Male;
  }
  workflow::WorkflowStateMachine skewed_state;
  AC_CHECK(skewed_state.BeginPreview());
  Config large_house_config;
  large_house_config.room_planning.default_soft_capacity = 40;
  const auto skewed = workflow::PreviewBuilder(skewed_sexes, large_house_config).Build(
      34, workflow::WorkflowCapability::MoveOnly, skewed_state);
  AC_CHECK(static_cast<bool>(skewed));
  std::unordered_map<snapshot::CatId, snapshot::RoomId> skewed_final;
  for (const auto &cat : skewed.value.snapshot.cats) {
    skewed_final.emplace(cat.id, *cat.room_id);
  }
  for (const auto &move : skewed.value.room_plan.moves) {
    skewed_final.at(move.cat_id) = move.to_room;
  }
  std::unordered_map<snapshot::RoomId, std::size_t> skewed_counts;
  for (const auto &cat : skewed.value.snapshot.cats) {
    const auto &room = skewed_final.at(cat.id);
    ++skewed_counts[room];
  }
  AC_CHECK(skewed_counts.size() == 4);
  AC_CHECK(std::ranges::none_of(
      skewed.value.room_plan.moves,
      [](const auto &move) {
        return move.reason == "sex-balance" ||
            move.reason == "sex-balance-and-potential-room";
      }));

  WorkflowReadFake breeding_sexes;
  breeding_sexes.house = skewed_sexes.house;
  ConfigureRoomPurposes(breeding_sexes.house);
  ConfigureBreedingPair(breeding_sexes.house, 1, 9);
  workflow::WorkflowStateMachine breeding_state;
  AC_CHECK(breeding_state.BeginPreview());
  const auto breeding = workflow::PreviewBuilder(breeding_sexes, large_house_config).Build(
      35, workflow::WorkflowCapability::MoveOnly, breeding_state);
  AC_CHECK(static_cast<bool>(breeding));
  const auto breeding_final = FinalRooms(breeding.value);
  const auto [breeding_female, breeding_male] = SexCounts(
      breeding.value, breeding_final, "Attic");
  // Only this pair has known pedigree compatibility in the fixture.
  AC_CHECK(breeding_female == 1);
  AC_CHECK(breeding_male == 1);
  AC_CHECK(breeding_final.at(1) == "Attic");
  AC_CHECK(breeding_final.at(9) == "Attic");
  AC_CHECK(std::ranges::all_of(
      breeding.value.room_plan.moves,
      [](const auto& move) {
        const bool sex_balance = move.reason == "sex-balance" ||
            move.reason == "sex-balance-and-potential-room";
        return !sex_balance || move.to_room == "Attic";
      }));
  ApplyRooms(breeding_sexes.house, breeding_final);
  workflow::WorkflowStateMachine breeding_repeated_state;
  AC_CHECK(breeding_repeated_state.BeginPreview());
  const auto breeding_repeated =
      workflow::PreviewBuilder(breeding_sexes, large_house_config).Build(
          36,
          workflow::WorkflowCapability::MoveOnly,
          breeding_repeated_state);
  AC_CHECK(static_cast<bool>(breeding_repeated));
  AC_CHECK(breeding_repeated.value.room_plan.moves.empty());

  WorkflowReadFake pair_pool;
  pair_pool.house = WorkflowHouse(8);
  pair_pool.house.rooms.front().id = "Floor1_Large";
  pair_pool.house.rooms.push_back({.id = "Attic"});
  pair_pool.house.capabilities.read_room_attributes = true;
  pair_pool.house.capabilities.read_sexuality = true;
  pair_pool.house.capabilities.read_relationships = true;
  Room(pair_pool.house, "Attic").attributes =
      snapshot::RoomAttributes{.comfort = 30, .stimulation = 40};
  Room(pair_pool.house, "Floor1_Large").attributes =
      snapshot::RoomAttributes{.comfort = 5, .stimulation = 5};
  for (auto& cat : pair_pool.house.cats) {
    cat.room_id = "Floor1_Large";
    cat.sex = cat.id <= 4
        ? snapshot::CatSex::Female
        : snapshot::CatSex::Male;
    cat.sexuality = snapshot::CatSexuality::Straight;
    cat.sexuality_coefficient = 0.0;
    for (auto& stat : cat.genetic_stats.values) {
      stat = 6;
    }
  }
  Room(pair_pool.house, "Floor1_Large").residents =
      {1, 2, 3, 4, 5, 6, 7, 8};
  pair_pool.house.pedigree_pair_coefficients = {
      {1, 5, 0.0},
      {2, 6, 0.01},
      {2, 5, 0.9},
      {1, 6, 0.9},
      {3, 7, 0.1},
      {3, 5, 0.1},
      {1, 7, 0.1},
      {4, 8, 0.2},
      {4, 5, 0.2},
      {1, 8, 0.2}
  };
  workflow::WorkflowStateMachine pair_pool_state;
  AC_CHECK(pair_pool_state.BeginPreview());
  Config pair_pool_config;
  pair_pool_config.room_planning.avoid_inbreeding_pairs = false;
  const auto pair_pool_plan = workflow::PreviewBuilder(pair_pool, pair_pool_config).Build(
      41, workflow::WorkflowCapability::MoveOnly, pair_pool_state);
  AC_CHECK(static_cast<bool>(pair_pool_plan));
  const auto pair_pool_final = FinalRooms(pair_pool_plan.value);
  AC_CHECK(pair_pool_final.at(1) == "Attic");
  AC_CHECK(pair_pool_final.at(5) == "Attic");
  AC_CHECK(pair_pool_final.at(3) == "Attic");
  AC_CHECK(pair_pool_final.at(7) == "Attic");
  AC_CHECK(pair_pool_final.at(2) != "Attic");
  AC_CHECK(pair_pool_final.at(6) != "Attic");
  AC_CHECK(std::ranges::count_if(
      pair_pool_plan.value.room_plan.moves,
      [](const auto& move) {
        return move.reason == "compatible-breeding-pool";
      }) == 2);

  // No parent is all-seven. Complementary groups can reach all seven,
  // whereas adding an ordinary cat would make some crosses unable to do so.
  // Vary both the population and the compatible group: this is not a
  // fixed two-cat room, and a second arrangement must remain idempotent.
  for (const auto [population, pool_size] :
       {std::pair{16U, 4U}, std::pair{24U, 6U}}) {
    WorkflowReadFake quality_pool;
    quality_pool.house = WorkflowHouse(population);
    auto& house = quality_pool.house;
    house.rooms.front().id = "Floor1_Large";
    house.rooms.push_back({.id = "Attic"});
    house.capabilities.read_room_attributes = true;
    house.capabilities.read_sexuality = true;
    house.capabilities.read_relationships = true;
    Room(house, "Attic").attributes =
        snapshot::RoomAttributes{.comfort = 30, .stimulation = 40};
    Room(house, "Floor1_Large").attributes =
        snapshot::RoomAttributes{.comfort = 5, .stimulation = 5};
    for (auto& cat : house.cats) {
      cat.room_id = "Floor1_Large";
      cat.sex = cat.id % 2 == 1 ? snapshot::CatSex::Female
                              : snapshot::CatSex::Male;
      cat.sexuality = snapshot::CatSexuality::Straight;
      cat.sexuality_coefficient = 0.0;
      for (std::size_t i = 0; i < snapshot::kStatCount; ++i) {
        cat.genetic_stats.values[i] = cat.id > pool_size ? 4 :
            ((i < 4) == (cat.sex == snapshot::CatSex::Female) ? 7 : 6);
      }
      for (const auto& other : house.cats) {
        if (other.id > cat.id) {
          house.pedigree_pair_coefficients.push_back({cat.id, other.id, 0.0});
        }
      }
    }
    workflow::WorkflowStateMachine quality_state;
    AC_CHECK(quality_state.BeginPreview());
    Config quality_config;
    quality_config.room_planning.default_soft_capacity = population;
    quality_config.room_planning.breeding_room_population = pool_size;
    const auto quality_plan = workflow::PreviewBuilder(quality_pool, quality_config).Build(
        42, workflow::WorkflowCapability::MoveOnly, quality_state);
    AC_CHECK(static_cast<bool>(quality_plan));
    const auto final = FinalRooms(quality_plan.value);
    for (const auto& cat : house.cats) {
      AC_CHECK((final.at(cat.id) == "Attic") == (cat.id <= pool_size));
    }
    ApplyRooms(house, final);
    workflow::WorkflowStateMachine repeat_state;
    AC_CHECK(repeat_state.BeginPreview());
    const auto repeated = workflow::PreviewBuilder(quality_pool, quality_config).Build(
        43, workflow::WorkflowCapability::MoveOnly, repeat_state);
    AC_CHECK(static_cast<bool>(repeated));
    AC_CHECK(repeated.value.room_plan.moves.empty());

    const auto with_settings = [&](const Config& config) {
      workflow::WorkflowStateMachine settings_state;
      AC_CHECK(settings_state.BeginPreview());
      return workflow::PreviewBuilder(quality_pool, config).Build(
          44, workflow::WorkflowCapability::MoveOnly, settings_state);
    };
    // Requested population controls the actual room, including expansion
    // beyond the original balanced occupancy. Lower-coverage candidates may
    // fill the group; eligibility and pedigree rules still apply.
    for (const auto requested : {2U, 4U, 8U, 12U}) {
      auto expanded_config = quality_config;
      expanded_config.room_planning.breeding_room_population = requested;
      const auto expanded = with_settings(expanded_config);
      AC_CHECK(expanded.value.room_plan.validation_errors.empty());
      const auto expanded_rooms = FinalRooms(expanded.value);
      AC_CHECK(std::ranges::count_if(expanded_rooms, [](const auto& entry) {
        return entry.second == "Attic";
      }) == requested);
      const auto [females, males] = SexCounts(expanded.value, expanded_rooms, "Attic");
      AC_CHECK(females == requested / 2);
      AC_CHECK(males == requested / 2);
    }
    auto settings = quality_config;
    settings.room_planning.keep_breeding_pairs_together = false;
    settings.room_planning.prefer_single_combat_staging_room = false;
    const auto ungrouped = with_settings(settings);
    AC_CHECK(static_cast<bool>(ungrouped));
    const auto ungrouped_rooms = FinalRooms(ungrouped.value);
    AC_CHECK(std::ranges::count_if(ungrouped_rooms, [](const auto& entry) {
      return entry.second == "Attic";
    }) == population / 2);
    AC_CHECK(std::ranges::none_of(ungrouped.value.room_plan.moves,
        [](const auto& move) {
          return move.reason == "recommended-breeding-pair" ||
              move.reason == "compatible-breeding-pool";
        }));

    settings.room_planning.default_soft_capacity = population / 2 - 1;
    settings.room_planning.allow_soft_overflow = false;
    const auto too_small = with_settings(settings);
    AC_CHECK(static_cast<bool>(too_small));
    AC_CHECK(!too_small.value.room_plan.validation_errors.empty());
    AC_CHECK(!too_small.value.room_plan.move_execution_allowed);
    AC_CHECK(std::ranges::find(too_small.value.preview.warnings,
        "configured-or-hard-room-capacity-insufficient") !=
        too_small.value.preview.warnings.end());
    settings.room_planning.allow_soft_overflow = true;
    settings.room_planning.max_soft_overflow_per_room = 0;
    const auto no_overflow_space = with_settings(settings);
    AC_CHECK(no_overflow_space.value.room_plan.validation_errors.empty());
    AC_CHECK(std::ranges::find(no_overflow_space.value.preview.warnings,
        "room-crowding-advisory-threshold-exceeded") !=
        no_overflow_space.value.preview.warnings.end());
    settings.room_planning.max_soft_overflow_per_room = 1;
    const auto fits_overflow = with_settings(settings);
    AC_CHECK(fits_overflow.value.room_plan.validation_errors.empty());
    AC_CHECK(std::ranges::find(fits_overflow.value.preview.warnings,
        "room-crowding-advisory-threshold-exceeded") ==
        fits_overflow.value.preview.warnings.end());
    settings.room_planning.allow_soft_overflow = false;
    ++settings.room_planning.default_soft_capacity;
    const auto fits_soft = with_settings(settings);
    AC_CHECK(fits_soft.value.room_plan.validation_errors.empty());

    settings.breeding_scoring.core_breeders = 0;
    settings.breeding_scoring.reserve_breeders = 0;
    const auto pair_only_core = with_settings(settings);
    AC_CHECK(pair_only_core.value.preview.breeding_core_count == 2);
    AC_CHECK(pair_only_core.value.preview.breeding_reserve_count == 0);
    settings.breeding_scoring.core_breeders = 8;
    settings.breeding_scoring.reserve_breeders = 4;
    const auto larger_pools = with_settings(settings);
    AC_CHECK(larger_pools.value.preview.breeding_core_count == 8);
    AC_CHECK(larger_pools.value.preview.breeding_reserve_count == 4);
    settings.breeding_scoring.core_breeders = 0;
    settings.breeding_scoring.reserve_breeders = 0;
    settings.classification.minimum_breeding_pool = 0;
    const auto no_retention = with_settings(settings);
    AC_CHECK(std::ranges::count_if(no_retention.value.classification.decisions,
        [](const auto& d) { return d.breeding_pool_protected; }) == 0);
    settings.classification.minimum_breeding_pool = 12;
    const auto retention = with_settings(settings);
    AC_CHECK(std::ranges::count_if(retention.value.classification.decisions,
        [](const auto& d) { return d.breeding_pool_protected; }) == 12);

    // No known unrelated pair remains: enabled must not silently choose
    // a related pair; disabled retains the existing COI ranking penalty.
    for (auto& coefficient : house.pedigree_pair_coefficients) {
      coefficient.coefficient = 0.125;
    }
    settings.room_planning.keep_breeding_pairs_together = true;
    settings.room_planning.default_soft_capacity = population;
    const auto avoid_related = with_settings(settings);
    AC_CHECK(avoid_related.value.classification.breeding_pair_preferences.empty());
    settings.room_planning.avoid_inbreeding_pairs = false;
    const auto allow_related = with_settings(settings);
    AC_CHECK(!allow_related.value.classification.breeding_pair_preferences.empty());
    AC_CHECK(std::ranges::any_of(allow_related.value.classification.decisions,
        [](const auto& decision) { return decision.breeding_partner_id.has_value(); }));
  }

  WorkflowReadFake constrained;
  constrained.house = WorkflowHouse(18);
  constrained.house.rooms.front().id = "Floor1_Large";
  constrained.house.rooms.push_back({.id = "Attic"});
  constrained.house.rooms.push_back({.id = "Floor1_Small"});
  for (auto& cat : constrained.house.cats) {
    cat.sex = cat.id <= 9
        ? snapshot::CatSex::Female
        : snapshot::CatSex::Male;
    cat.room_id = cat.id >= 11 && cat.id <= 14
        ? "Floor1_Small"
        : "Floor1_Large";
  }
  FinalRoomMap constrained_current;
  for (const auto& cat : constrained.house.cats) {
    constrained_current.emplace(cat.id, *cat.room_id);
  }
  ApplyRooms(constrained.house, constrained_current);
  ConfigureRoomPurposes(constrained.house);
  ConfigureBreedingPair(constrained.house, 1, 10);
  const auto protection_path = std::filesystem::temp_directory_path() /
      "auto_cattery_breeding_balance_protection_test.json";
  {
    std::ofstream output(protection_path, std::ios::trunc);
    output << R"({"schema_version":1,"records":[)"
           << R"({"cat_id":1,"level":"NoCull","fixed_room":"Floor1_Small"},)"
           << R"({"cat_id":11,"level":"NoMove"},)"
           << R"({"cat_id":12,"level":"NoMove"},)"
           << R"({"cat_id":13,"level":"NoMove"},)"
           << R"({"cat_id":14,"level":"NoMove"})"
           << R"(],"blacklist":[]})";
  }
  workflow::WorkflowStateMachine constrained_state;
  AC_CHECK(constrained_state.BeginPreview());
  const auto constrained_plan = workflow::PreviewBuilder(
      constrained, Config{}, protection_path).Build(
          37,
          workflow::WorkflowCapability::MoveOnly,
          constrained_state);
  AC_CHECK(static_cast<bool>(constrained_plan));
  const auto constrained_final = FinalRooms(constrained_plan.value);
  const auto [constrained_female, constrained_male] = SexCounts(
      constrained_plan.value,
      constrained_final,
      "Floor1_Small");
  AC_CHECK(constrained_female >= 1);
  AC_CHECK(constrained_male >= 4);
  for (snapshot::CatId id = 11; id <= 14; ++id) {
    AC_CHECK(constrained_final.at(id) == "Floor1_Small");
  }
  AC_CHECK(constrained_final.at(1) == "Floor1_Small");
  // A fixed parent outside the attic cannot silently redefine breeding room.

  AC_CHECK(std::ranges::find(
      constrained_plan.value.room_plan.limitations,
      "breeding-pair-room-unavailable") !=
      constrained_plan.value.room_plan.limitations.end());
  std::error_code ignored;
  std::filesystem::remove(protection_path, ignored);

  WorkflowReadFake purpose_aware;
  purpose_aware.house = WorkflowHouse(12);
  purpose_aware.house.rooms.front().id = "Floor1_Large";
  purpose_aware.house.rooms.push_back({.id = "Attic"});
  purpose_aware.house.rooms.push_back({.id = "Floor1_Small"});
  purpose_aware.house.rooms.push_back({.id = "Floor2_Large"});
  for (auto& cat : purpose_aware.house.cats) {
    cat.room_id = "Attic";
    cat.sex = cat.id % 2 == 0
        ? snapshot::CatSex::Male
        : snapshot::CatSex::Female;
    if (cat.id >= 10) {
      cat.life_stage = snapshot::LifeStage::Kitten;
    }
  }
  ApplyRooms(purpose_aware.house, FinalRoomMap{
      {1, "Attic"}, {2, "Attic"}, {3, "Attic"}, {4, "Attic"},
      {5, "Attic"}, {6, "Attic"}, {7, "Attic"}, {8, "Attic"},
      {9, "Attic"}, {10, "Attic"}, {11, "Attic"}, {12, "Attic"}});
  purpose_aware.house.capabilities.read_room_attributes = true;
  Room(purpose_aware.house, "Attic").attributes =
      snapshot::RoomAttributes{
          .comfort = 30, .stimulation = 40, .health = 0};
  Room(purpose_aware.house, "Floor1_Small").attributes =
      snapshot::RoomAttributes{
          .comfort = 20, .stimulation = 20, .health = 50};
  Room(purpose_aware.house, "Floor1_Large").attributes =
      snapshot::RoomAttributes{
          .comfort = -15, .stimulation = 100, .health = 0};
  Room(purpose_aware.house, "Floor2_Large").attributes =
      snapshot::RoomAttributes{
          .comfort = 10, .stimulation = 10, .health = 35};
  ConfigureBreedingPair(purpose_aware.house, 1, 2);
  workflow::WorkflowStateMachine purpose_state;
  AC_CHECK(purpose_state.BeginPreview());
  const auto purpose_plan = workflow::PreviewBuilder(purpose_aware).Build(
      38, workflow::WorkflowCapability::MoveOnly, purpose_state);
  AC_CHECK(static_cast<bool>(purpose_plan));
  const auto purpose_final = FinalRooms(purpose_plan.value);
  AC_CHECK(purpose_final.at(1) == "Attic");
  AC_CHECK(purpose_final.at(2) == "Attic");
  AC_CHECK(purpose_final.at(10) == "Floor1_Small");
  AC_CHECK(purpose_final.at(11) == "Floor1_Small");
  AC_CHECK(purpose_final.at(12) == "Floor1_Small");
  for (const auto& decision : purpose_plan.value.classification.decisions) {
    if (decision.cat_id == 1 || decision.cat_id == 2 ||
        decision.cat_id >= 10 ||
        decision.primary_role !=
            classification::CatRole::CombatRecommended) {
      continue;
    }
    AC_CHECK(purpose_final.at(decision.cat_id) == "Floor1_Large");
  }
  for (snapshot::CatId id = 3; id <= 9; ++id) {
    AC_CHECK(purpose_final.at(id) == "Floor1_Large");
  }
  AC_CHECK(std::ranges::count_if(
      purpose_plan.value.room_plan.moves,
      [](const auto& move) {
        return move.reason == "kitten-nursery-room";
      }) == 3);

  // Room purposes depend on furniture, not current resident counts. Test
  // all supported room counts, even with another room better for breeding.
  for (const auto room_count : {2U, 3U, 4U, 5U}) {
    WorkflowReadFake layout;
    layout.house = purpose_aware.house;
    if (room_count == 5) {
      layout.house.rooms.push_back({.id = "Floor2_Small",
        .attributes = snapshot::RoomAttributes{.comfort = 25, .health = 5}});
    }
    std::erase_if(layout.house.rooms, [&](const auto& room) {
      return (room_count < 4 && room.id == "Floor2_Large") ||
             (room_count < 3 && room.id == "Floor1_Small");
    });
    Room(layout.house, "Floor1_Large").attributes->stimulation = 200;
    Room(layout.house, "Attic").attributes->stimulation = 1;
    workflow::WorkflowStateMachine layout_state;
    AC_CHECK(layout_state.BeginPreview());
    const auto plan = workflow::PreviewBuilder(layout).Build(
        50 + room_count, workflow::WorkflowCapability::MoveOnly, layout_state);
    AC_CHECK(plan.value.room_plan.validation_errors.empty());
    const auto rooms = FinalRooms(plan.value);
    AC_CHECK(rooms.at(1) == "Attic");
    AC_CHECK(rooms.at(2) == "Attic");
    for (snapshot::CatId id = 3; id <= 9; ++id) {
      AC_CHECK(rooms.at(id) == "Floor1_Large");
    }
    for (snapshot::CatId id = 10; id <= 12; ++id) {
      AC_CHECK(rooms.at(id) == (room_count == 2 ? "Floor1_Large" : "Floor1_Small"));
    }
    ApplyRooms(layout.house, rooms);
    workflow::WorkflowStateMachine repeat_layout;
    AC_CHECK(repeat_layout.BeginPreview());
    const auto again = workflow::PreviewBuilder(layout).Build(
        60 + room_count, workflow::WorkflowCapability::MoveOnly, repeat_layout);
    AC_CHECK(again.value.room_plan.moves.empty());
  }

  Config shared_room_config;
  shared_room_config.room_planning.keep_kittens_separate_when_possible = false;
  workflow::WorkflowStateMachine shared_room_state;
  AC_CHECK(shared_room_state.BeginPreview());
  const auto shared_room_plan = workflow::PreviewBuilder(
      purpose_aware, shared_room_config).Build(
          39, workflow::WorkflowCapability::MoveOnly, shared_room_state);
  AC_CHECK(static_cast<bool>(shared_room_plan));
  AC_CHECK(std::ranges::none_of(
      shared_room_plan.value.room_plan.moves,
      [](const auto& move) {
        return move.reason == "kitten-nursery-room";
      }));

  const auto role_capacity_path = std::filesystem::temp_directory_path() /
      "auto_cattery_role_capacity_protection_test.json";
  {
    std::ofstream output(role_capacity_path, std::ios::trunc);
    output << R"({"schema_version":1,"records":[)"
           << R"({"cat_id":9,"level":"NoCull","fixed_room":"Floor2_Large"})"
           << R"(],"blacklist":[]})";
  }
  workflow::WorkflowStateMachine role_capacity_state;
  AC_CHECK(role_capacity_state.BeginPreview());
  const auto role_capacity_plan = workflow::PreviewBuilder(
      purpose_aware, Config{}, role_capacity_path).Build(
          40,
          workflow::WorkflowCapability::MoveOnly,
          role_capacity_state);
  AC_CHECK(static_cast<bool>(role_capacity_plan));
  for (const auto& error :
       role_capacity_plan.value.room_plan.validation_errors) {
    std::cerr << "role capacity validation: " << error << '\n';
  }
  AC_CHECK(role_capacity_plan.value.room_plan.validation_errors.empty());
  const auto nursery_move_count = std::ranges::count_if(
      role_capacity_plan.value.room_plan.moves,
      [](const auto& move) {
        return move.reason == "kitten-nursery-room";
      });
  if (nursery_move_count != 3) {
    for (const auto& move : role_capacity_plan.value.room_plan.moves) {
      std::cerr << "role capacity move: cat=" << move.cat_id
                << " to=" << move.to_room
                << " reason=" << move.reason << '\n';
    }
  }
  AC_CHECK(nursery_move_count == 3);
  std::filesystem::remove(role_capacity_path, ignored);

  for (auto &room : reader.house.rooms) {
    room.residents.clear();
  }
  for (auto &cat : reader.house.cats) {
    cat.room_id = final_room.at(cat.id);
    const auto room = std::ranges::find_if(
        reader.house.rooms,
        [&](const auto &candidate) {
          return candidate.id == *cat.room_id;
        });
    AC_CHECK(room != reader.house.rooms.end());
    room->residents.push_back(cat.id);
  }
  workflow::WorkflowStateMachine repeated_state;
  AC_CHECK(repeated_state.BeginPreview());
  const auto repeated = workflow::PreviewBuilder(reader).Build(
      32, workflow::WorkflowCapability::MoveOnly, repeated_state);
  AC_CHECK(static_cast<bool>(repeated));
  AC_CHECK(repeated.value.room_plan.moves.empty());

  WorkflowReadFake outside;
  outside.house = WorkflowHouse(8);
  outside.house.rooms.front().id = "Floor1_Large";
  for (auto &cat : outside.house.cats) {
    cat.room_id = "Floor1_Large";
  }
  outside.house.rooms.front().residents.erase(
      outside.house.rooms.front().residents.begin());
  outside.house.cats.front().room_id.reset();
  outside.house.rooms.push_back({.id = "Attic"});
  workflow::WorkflowStateMachine outside_state;
  AC_CHECK(outside_state.BeginPreview());
  const auto outside_built = workflow::PreviewBuilder(outside).Build(
      33, workflow::WorkflowCapability::MoveOnly, outside_state);
  AC_CHECK(static_cast<bool>(outside_built));
  AC_CHECK(outside_built.value.room_plan.moves.size() == 5);
  const auto outside_move = std::ranges::find_if(
      outside_built.value.room_plan.moves,
      [](const auto &move) { return move.cat_id == 1; });
  AC_CHECK(outside_move != outside_built.value.room_plan.moves.end());
  AC_CHECK(outside_move->from_room == "Outside");
  AC_CHECK(outside_move->executable);
}

}  // namespace autocattery::tests
