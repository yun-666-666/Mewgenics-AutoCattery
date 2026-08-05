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
  const auto skewed = workflow::PreviewBuilder(skewed_sexes).Build(
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
  const auto breeding = workflow::PreviewBuilder(breeding_sexes).Build(
      35, workflow::WorkflowCapability::MoveOnly, breeding_state);
  AC_CHECK(static_cast<bool>(breeding));
  const auto breeding_final = FinalRooms(breeding.value);
  const auto [breeding_female, breeding_male] = SexCounts(
      breeding.value, breeding_final, "Attic");
  AC_CHECK(breeding_female == 8);
  AC_CHECK(breeding_male == 14);
  AC_CHECK(SexCounts(
      breeding.value, breeding_final, "Floor1_Small").first == 0);
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
      workflow::PreviewBuilder(breeding_sexes).Build(
          36,
          workflow::WorkflowCapability::MoveOnly,
          breeding_repeated_state);
  AC_CHECK(static_cast<bool>(breeding_repeated));
  AC_CHECK(breeding_repeated.value.room_plan.moves.empty());

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
  AC_CHECK(constrained_female == 1);
  AC_CHECK(constrained_male == 5);
  AC_CHECK(constrained_final.at(1) == "Floor1_Small");
  AC_CHECK(constrained_final.at(10) == "Floor1_Small");
  AC_CHECK(std::ranges::find(
      constrained_plan.value.room_plan.limitations,
      "protected-residents-limit-breeding-sex-balance") !=
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
  AC_CHECK(purpose_final.at(10) == "Floor2_Large");
  AC_CHECK(purpose_final.at(11) == "Floor2_Large");
  AC_CHECK(purpose_final.at(12) == "Floor2_Large");
  std::size_t staged_combat_adults{};
  for (const auto& decision : purpose_plan.value.classification.decisions) {
    if (decision.cat_id == 1 || decision.cat_id == 2 ||
        decision.cat_id >= 10 ||
        decision.primary_role !=
            classification::CatRole::CombatRecommended) {
      continue;
    }
    staged_combat_adults +=
        purpose_final.at(decision.cat_id) == "Floor1_Small" ? 1U : 0U;
  }
  AC_CHECK(staged_combat_adults == 3);
  AC_CHECK(std::ranges::count_if(
      purpose_plan.value.room_plan.moves,
      [](const auto& move) {
        return move.reason == "kitten-nursery-room";
      }) == 3);

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
  if (nursery_move_count != 2) {
    for (const auto& move : role_capacity_plan.value.room_plan.moves) {
      std::cerr << "role capacity move: cat=" << move.cat_id
                << " to=" << move.to_room
                << " reason=" << move.reason << '\n';
    }
  }
  AC_CHECK(nursery_move_count == 2);
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
