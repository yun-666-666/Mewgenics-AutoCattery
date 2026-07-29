#pragma once

#include "auto_cattery/snapshot/game_read_adapter.hpp"

namespace autocattery::tests {

inline snapshot::HouseSnapshot WorkflowHouse(std::size_t cat_count = 3,
                                             std::uint64_t generation = 7) {
  snapshot::HouseSnapshot house;
  house.snapshot_id = 42;
  house.scene_generation = generation;
  house.game_day = 9;
  house.source_save_name = "sensitive-fixture-save";
  house.capabilities.stable_cat_id = true;
  house.capabilities.read_genetic_stats = true;
  house.capabilities.read_heredity_bonus = true;
  house.capabilities.read_equipment_bonus = true;
  house.capabilities.read_raw_ability_slots = true;
  house.capabilities.read_room_assignments = true;
  snapshot::RoomSnapshot room{.id = "Room-A"};
  for (std::size_t index = 0; index < cat_count; ++index) {
    snapshot::CatSnapshot cat;
    cat.id = static_cast<snapshot::CatId>(index + 1);
    cat.display_name = "private-cat-name";
    cat.room_id = room.id;
    cat.life_stage = snapshot::LifeStage::Adult;
    cat.available_for_combat = snapshot::TriState::Yes;
    cat.available_for_breeding = snapshot::TriState::Yes;
    cat.injured = snapshot::TriState::No;
    for (auto &value : cat.genetic_stats.values) {
      value = static_cast<std::int32_t>(10 + index);
    }
    for (auto &value : cat.heredity_bonus.values) {
      value = 1;
    }
    for (auto &value : cat.equipment_bonus.values) {
      value = 1;
    }
    room.residents.push_back(cat.id);
    house.cats.push_back(std::move(cat));
  }
  house.rooms.push_back(std::move(room));
  return house;
}

class WorkflowReadFake final : public snapshot::IGameReadAdapter {
public:
  Result<snapshot::HouseSnapshot>
  CaptureHouseSnapshot(std::uint64_t scene_generation) override {
    ++calls;
    if (failure != ErrorCode::Ok) {
      return {{}, failure, "synthetic capture failure"};
    }
    house.scene_generation = scene_generation;
    return {house};
  }

  snapshot::HouseSnapshot house{WorkflowHouse()};
  ErrorCode failure{ErrorCode::Ok};
  int calls{};
};

} // namespace autocattery::tests
