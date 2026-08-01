#include "auto_cattery/workflow/preview_builder.hpp"

#include <unordered_map>
#include <unordered_set>

#include "test_support.hpp"
#include "workflow_test_fixture.hpp"

namespace autocattery::tests {

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
  std::unordered_set<snapshot::RoomId> female_rooms;
  std::unordered_set<snapshot::RoomId> male_rooms;
  for (const auto &cat : built.value.snapshot.cats) {
    const auto &room = final_room.at(cat.id);
    ++counts[room];
    if (cat.sex == snapshot::CatSex::Female) {
      female_rooms.insert(room);
    } else if (cat.sex == snapshot::CatSex::Male) {
      male_rooms.insert(room);
    }
  }
  AC_CHECK(counts.size() == 4);
  AC_CHECK(std::ranges::all_of(
      counts,
      [](const auto &entry) { return entry.second == 3; }));
  AC_CHECK(female_rooms.size() == 2);
  AC_CHECK(male_rooms.size() == 4);

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
}

}  // namespace autocattery::tests
