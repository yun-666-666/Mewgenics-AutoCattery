#include "auto_cattery/workflow/preview_builder.hpp"

#include <algorithm>
#include <chrono>

#include "test_support.hpp"
#include "workflow_test_fixture.hpp"

namespace autocattery::tests {

void RunWorkflowPreviewBuilderTests() {
  WorkflowReadFake reader;
  workflow::WorkflowStateMachine state;
  AC_CHECK(state.BeginPreview());
  workflow::PreviewBuilder builder(reader);
  const auto built =
      builder.Build(18, workflow::WorkflowCapability::PreviewOnly, state);
  AC_CHECK(static_cast<bool>(built));
  AC_CHECK(reader.calls == 1);
  AC_CHECK(state.State() == workflow::WorkflowState::AwaitingConfirmation);
  AC_CHECK(built.value.preview.cat_count == 3);
  AC_CHECK(built.value.preview.room_count == 1);
  AC_CHECK(built.value.preview.game_data_modified == false);
  AC_CHECK(built.value.preview.capability ==
           workflow::WorkflowCapability::PreviewOnly);
  AC_CHECK(built.value.preview.summary.find("private-cat-name") ==
           std::string::npos);
  AC_CHECK(built.value.preview.summary.find("sensitive-fixture-save") ==
           std::string::npos);
  AC_CHECK(!built.value.preview.bindings.snapshot_content_digest.empty());
  AC_CHECK(!built.value.preview.bindings.classification_digest.empty());
  AC_CHECK(!built.value.preview.bindings.protection_digest.empty());
  AC_CHECK(!built.value.preview.bindings.room_plan_digest.empty());
  AC_CHECK(!built.value.preview.bindings.config_digest.empty());

  WorkflowReadFake potential;
  potential.house = WorkflowHouse(25);
  potential.house.rooms.front().id = "Floor1_Large";
  for (auto &cat : potential.house.cats) {
    cat.room_id = "Floor1_Large";
    cat.life_stage = snapshot::LifeStage::Unknown;
    cat.available_for_combat = snapshot::TriState::No;
    cat.available_for_breeding = snapshot::TriState::No;
    cat.injured = snapshot::TriState::Yes;
    cat.sex =
        cat.id == 1
            ? snapshot::CatSex::Male
            : snapshot::CatSex::Female;
  }
  Config potential_config;
  potential_config.combat_scoring.recommended_count = 10;
  workflow::WorkflowStateMachine potential_state;
  AC_CHECK(potential_state.BeginPreview());
  const auto potential_built =
      workflow::PreviewBuilder(potential, potential_config).Build(
          23, workflow::WorkflowCapability::MoveOnly, potential_state);
  AC_CHECK(static_cast<bool>(potential_built));
  AC_CHECK(potential_built.value.preview.room_count == 2);
  AC_CHECK(
      potential_built.value.preview.recommended_combat_count == 10);
  AC_CHECK(potential_built.value.preview.breeding_core_count == 0);
  AC_CHECK(potential_built.value.preview.planned_move_count == 13);
  AC_CHECK(potential_built.value.room_plan.move_execution_allowed);
  AC_CHECK(std::ranges::all_of(
      potential_built.value.room_plan.moves,
      [](const auto &move) {
        return move.to_room == "Attic" && move.executable;
      }));
  AC_CHECK(std::ranges::any_of(
      potential_built.value.preview.warnings,
      [](const auto &warning) {
        return warning == "potential-room-sex-mix-adjusted";
      }));

  WorkflowReadFake balanced_sexes;
  balanced_sexes.house = WorkflowHouse(8);
  balanced_sexes.house.rooms.front().id = "Floor1_Large";
  for (std::size_t index = 0;
       index < balanced_sexes.house.cats.size();
       ++index) {
    auto &cat = balanced_sexes.house.cats[index];
    cat.room_id = "Floor1_Large";
    cat.sex = index % 2 == 0
                  ? snapshot::CatSex::Female
                  : snapshot::CatSex::Male;
  }
  Config balanced_config;
  balanced_config.combat_scoring.recommended_count = 4;
  workflow::WorkflowStateMachine balanced_state;
  AC_CHECK(balanced_state.BeginPreview());
  const auto balanced =
      workflow::PreviewBuilder(balanced_sexes, balanced_config).Build(
          24, workflow::WorkflowCapability::MoveOnly, balanced_state);
  AC_CHECK(static_cast<bool>(balanced));
  AC_CHECK(balanced.value.preview.planned_move_count == 4);
  AC_CHECK(balanced.value.room_plan.algorithm_version ==
           room_planning::kBalancedMoveOnlyAlgorithmVersion);
  AC_CHECK(std::ranges::find(
      balanced.value.room_plan.limitations,
      "room-capacities-unknown") !=
           balanced.value.room_plan.limitations.end());
  std::size_t attic_female{};
  std::size_t attic_male{};
  for (const auto &move : balanced.value.room_plan.moves) {
    if (move.to_room != "Attic") {
      continue;
    }
    const auto cat = std::ranges::find_if(
        balanced.value.snapshot.cats,
        [&](const auto &candidate) {
          return candidate.id == move.cat_id;
        });
    AC_CHECK(cat != balanced.value.snapshot.cats.end());
    attic_female += cat->sex == snapshot::CatSex::Female ? 1U : 0U;
    attic_male += cat->sex == snapshot::CatSex::Male ? 1U : 0U;
  }
  AC_CHECK(attic_female > 0);
  AC_CHECK(attic_male > 0);

  WorkflowReadFake four_rooms;
  four_rooms.house = WorkflowHouse(10);
  four_rooms.house.rooms.front().id = "Floor1_Large";
  four_rooms.house.rooms.push_back({.id = "Attic"});
  four_rooms.house.rooms.push_back({.id = "Floor1_Small"});
  four_rooms.house.rooms.push_back({.id = "Floor2_Large"});
  for (auto &cat : four_rooms.house.cats) {
    cat.room_id = "Floor1_Large";
    cat.sex = cat.id % 2 == 0
                  ? snapshot::CatSex::Female
                  : snapshot::CatSex::Male;
  }
  workflow::WorkflowStateMachine four_room_state;
  AC_CHECK(four_room_state.BeginPreview());
  const auto four_room_plan =
      workflow::PreviewBuilder(four_rooms).Build(
          25, workflow::WorkflowCapability::MoveOnly,
          four_room_state);
  AC_CHECK(static_cast<bool>(four_room_plan));
  AC_CHECK(four_room_plan.value.preview.room_count == 4);
  AC_CHECK(four_room_plan.value.preview.planned_move_count == 7);
  AC_CHECK(std::ranges::all_of(
      four_room_plan.value.room_plan.moves,
      [](const auto &move) {
        return move.from_room == "Floor1_Large" &&
               move.to_room != "Floor1_Large" && move.executable;
      }));

  WorkflowReadFake empty;
  empty.house = WorkflowHouse(0);
  workflow::WorkflowStateMachine empty_state;
  AC_CHECK(empty_state.BeginPreview());
  const auto empty_built = workflow::PreviewBuilder(empty).Build(
      19, workflow::WorkflowCapability::PreviewOnly, empty_state);
  AC_CHECK(static_cast<bool>(empty_built));
  AC_CHECK(empty_built.value.preview.cat_count == 0);

  for (const auto count : {std::size_t{100}, std::size_t{1000}}) {
    WorkflowReadFake scale;
    scale.house = WorkflowHouse(count);
    workflow::WorkflowStateMachine scale_state;
    AC_CHECK(scale_state.BeginPreview());
    const auto started = std::chrono::steady_clock::now();
    const auto scaled = workflow::PreviewBuilder(scale).Build(
        21, workflow::WorkflowCapability::PreviewOnly, scale_state);
    const auto elapsed = std::chrono::steady_clock::now() - started;
    AC_CHECK(static_cast<bool>(scaled));
    AC_CHECK(scaled.value.preview.cat_count == count);
    AC_CHECK(elapsed < std::chrono::seconds(2));
  }

  WorkflowReadFake partial;
  partial.house = WorkflowHouse();
  partial.house.rooms.front().residents.pop_back();
  partial.house.cats.back().room_id.reset();
  workflow::WorkflowStateMachine partial_state;
  AC_CHECK(partial_state.BeginPreview());
  const auto partial_result = workflow::PreviewBuilder(partial).Build(
      22, workflow::WorkflowCapability::PreviewOnly, partial_state);
  AC_CHECK(static_cast<bool>(partial_result));
  AC_CHECK(partial_result.value.preview.unplaced_count == 1);
  AC_CHECK(!partial_result.value.preview.fully_satisfied);

  WorkflowReadFake failing;
  failing.failure = ErrorCode::CatDataUnavailable;
  workflow::WorkflowStateMachine failed_state;
  AC_CHECK(failed_state.BeginPreview());
  const auto failed = workflow::PreviewBuilder(failing).Build(
      20, workflow::WorkflowCapability::PreviewOnly, failed_state);
  AC_CHECK(!static_cast<bool>(failed));
  AC_CHECK(failed_state.State() == workflow::WorkflowState::Failed);
}

} // namespace autocattery::tests
