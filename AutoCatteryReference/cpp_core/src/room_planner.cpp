#include "autocattery/room_planner.hpp"
#include <set>
#include <tuple>
#include <unordered_map>

namespace autocattery {
namespace {

int Priority(const CatDecision& decision) {
  switch (decision.primary_role) {
    case CatRole::BreedingCore: return 0;
    case CatRole::CombatRecommended: return 1;
    case CatRole::BreedingReserve: return 3;
    case CatRole::GeneralReserve: return 4;
    default: return 5;
  }
}

std::string RoleName(CatRole role) {
  switch (role) {
    case CatRole::BreedingCore: return "breeding_core";
    case CatRole::CombatRecommended: return "combat_recommended";
    case CatRole::BreedingReserve: return "breeding_reserve";
    case CatRole::GeneralReserve: return "general_reserve";
    case CatRole::CullCandidate: return "cull_candidate";
    case CatRole::ProtectedUnmanaged: return "protected_unmanaged";
    default: return "ineligible";
  }
}

bool Contains(const std::vector<CatId>& values, CatId id) {
  return std::find(values.begin(), values.end(), id) != values.end();
}

}  // namespace

RoomPlan PlanRooms(const HouseSnapshot& house,
                   const ClassificationPlan& classification,
                   const RoomPlanningConfig& config) {
  RoomPlan output;
  output.snapshot_id = house.snapshot_id;

  std::unordered_map<RoomId, const RoomSnapshot*> rooms;
  std::unordered_map<CatId, const CatSnapshot*> cats;
  std::unordered_map<CatId, const CatDecision*> decisions;
  std::unordered_map<RoomId, std::vector<CatId>> occupancy;
  for (const auto& room : house.rooms) {
    rooms[room.id] = &room;
    occupancy[room.id] = room.residents;
  }
  for (const auto& cat : house.cats) cats[cat.id] = &cat;
  for (const auto& decision : classification.decisions) decisions[decision.cat_id] = &decision;

  const auto capacity = [&](const RoomSnapshot& room) {
    if (room.hard_capacity) return *room.hard_capacity;
    const int soft = room.soft_capacity > 0 ? room.soft_capacity : config.default_soft_capacity;
    return soft + (config.allow_soft_overflow ? config.max_soft_overflow_per_room : 0);
  };

  const auto preserve_unknown_breeder = [&](CatId cat_id) {
    const auto& cat = *cats.at(cat_id);
    const auto& decision = *decisions.at(cat_id);
    const auto room_it = rooms.find(cat.room_id);
    return decision.primary_role == CatRole::BreedingCore &&
           room_it != rooms.end() &&
           room_it->second->allows_breeding &&
           cat.compatible_breeding_ids.empty();
  };

  std::vector<CatId> movable;
  for (const auto& cat : house.cats) {
    const auto& decision = *decisions.at(cat.id);
    const bool manageable_role = decision.primary_role != CatRole::CullCandidate &&
                                 decision.primary_role != CatRole::ProtectedUnmanaged &&
                                 decision.primary_role != CatRole::Ineligible;
    if (CanMove(decision.protection) && manageable_role && !preserve_unknown_breeder(cat.id)) {
      movable.push_back(cat.id);
    }
  }

  for (CatId cat_id : movable) {
    const RoomId room_id = cats.at(cat_id)->room_id;
    auto& residents = occupancy[room_id];
    residents.erase(std::remove(residents.begin(), residents.end(), cat_id), residents.end());
  }

  std::sort(movable.begin(), movable.end(), [&](CatId left, CatId right) {
    const int left_priority = Priority(*decisions.at(left));
    const int right_priority = Priority(*decisions.at(right));
    return left_priority == right_priority ? left < right : left_priority < right_priority;
  });

  std::unordered_map<CatId, RoomId> target;
  for (const auto& cat : house.cats) target[cat.id] = cat.room_id;

  for (CatId cat_id : movable) {
    const auto& cat = *cats.at(cat_id);
    const auto& decision = *decisions.at(cat_id);
    using Option = std::tuple<int, int, int, int, RoomId>;
    std::optional<Option> best;

    for (const auto& [room_id, room_ptr] : rooms) {
      const auto& room = *room_ptr;
      if (room.is_special_room || room.player_locked) continue;
      if (static_cast<int>(occupancy[room_id].size()) >= capacity(room)) continue;

      int role_penalty = 0;
      if (decision.primary_role == CatRole::BreedingCore) {
        role_penalty = room.allows_breeding ? 0 : 30;
      } else if (decision.combat_recommended) {
        role_penalty = room.room_type == "combat_staging" ? 0 : 10;
      } else if (cat.life_stage == LifeStage::Kitten) {
        role_penalty = room.allows_kittens ? 0 : 10000;
      } else {
        role_penalty = (room.room_type == "general" || room.room_type == "combat_staging") ? 0 : 5;
      }
      if (role_penalty >= 10000) continue;

      if (decision.primary_role == CatRole::BreedingCore && room.allows_breeding) {
        std::vector<CatId> existing_breeders;
        for (CatId resident : occupancy[room_id]) {
          const auto decision_it = decisions.find(resident);
          if (decision_it != decisions.end() && decision_it->second->breeding_core) {
            existing_breeders.push_back(resident);
          }
        }
        if (!existing_breeders.empty()) {
          if (cat.compatible_breeding_ids.empty()) continue;
          bool compatible = true;
          for (CatId existing : existing_breeders) {
            if (!Contains(cat.compatible_breeding_ids, existing)) {
              compatible = false;
              break;
            }
          }
          if (!compatible) continue;
        }
      }

      const int soft_capacity = room.soft_capacity > 0 ? room.soft_capacity : config.default_soft_capacity;
      const int overflow = std::max(0, static_cast<int>(occupancy[room_id].size()) + 1 - soft_capacity);
      const int move_penalty = room_id == cat.room_id ? 0 : 1;
      const Option option{role_penalty, overflow, move_penalty,
                          static_cast<int>(occupancy[room_id].size()), room_id};
      if (!best || option < *best) best = option;
    }

    if (!best) {
      output.unplaced_cats.push_back(cat_id);
      const RoomId old_room = cat.room_id;
      if (rooms.contains(old_room) && static_cast<int>(occupancy[old_room].size()) < capacity(*rooms.at(old_room))) {
        occupancy[old_room].push_back(cat_id);
      }
      continue;
    }

    const RoomId room_id = std::get<4>(*best);
    target[cat_id] = room_id;
    occupancy[room_id].push_back(cat_id);
  }

  for (const auto& [cat_id, room_id] : target) {
    const RoomId old_room = cats.at(cat_id)->room_id;
    if (old_room != room_id && !Contains(output.unplaced_cats, cat_id)) {
      const auto& decision = *decisions.at(cat_id);
      output.moves.push_back({cat_id, old_room, room_id, RoleName(decision.primary_role), Priority(decision)});
    }
  }

  std::vector<CatId> cull_candidates;
  for (const auto& decision : classification.decisions) {
    if (decision.primary_role == CatRole::CullCandidate && decision.destructive_action_allowed) {
      cull_candidates.push_back(decision.cat_id);
    }
  }
  std::sort(cull_candidates.begin(), cull_candidates.end(), [&](CatId left, CatId right) {
    const double left_value = std::max(decisions.at(left)->combat_score, decisions.at(left)->breeding_score);
    const double right_value = std::max(decisions.at(right)->combat_score, decisions.at(right)->breeding_score);
    return left_value == right_value ? left < right : left_value < right_value;
  });

  const auto relief_count = std::min(cull_candidates.size(), output.unplaced_cats.size());
  output.capacity_relief_culls.assign(cull_candidates.begin(), cull_candidates.begin() + relief_count);
  if (!output.unplaced_cats.empty()) {
    output.warnings.push_back("capacity insufficient; partial plan");
  }
  output.fully_satisfied = output.unplaced_cats.empty();
  std::sort(output.moves.begin(), output.moves.end(), [](const auto& left, const auto& right) {
    return left.cat_id < right.cat_id;
  });
  return output;
}

}  // namespace autocattery
