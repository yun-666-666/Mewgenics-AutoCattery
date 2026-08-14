#include "auto_cattery/furniture_analysis/service.hpp"

#include <algorithm>
#include <cstddef>
#include <map>
#include <sstream>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

#include "auto_cattery/workflow/digests.hpp"

namespace autocattery::furniture_analysis {
namespace {

void Add(
    snapshot::RoomAttributes& total,
    const snapshot::RoomAttributes& value) {
    total.comfort += value.comfort;
    total.stimulation += value.stimulation;
    total.health += value.health;
    total.mutation += value.mutation;
    total.appeal += value.appeal;
}

snapshot::RoomAttributes Subtract(
    const snapshot::RoomAttributes& improved,
    const snapshot::RoomAttributes& current) {
    return {
        .comfort = improved.comfort - current.comfort,
        .stimulation = improved.stimulation - current.stimulation,
        .health = improved.health - current.health,
        .mutation = improved.mutation - current.mutation,
        .appeal = improved.appeal - current.appeal};
}

bool Dominates(
    const snapshot::RoomAttributes& improved,
    const snapshot::RoomAttributes& current) {
    const bool no_worse =
        improved.comfort >= current.comfort &&
        improved.stimulation >= current.stimulation &&
        improved.health >= current.health &&
        improved.mutation >= current.mutation &&
        improved.appeal >= current.appeal;
    const bool strictly_better =
        improved.comfort > current.comfort ||
        improved.stimulation > current.stimulation ||
        improved.health > current.health ||
        improved.mutation > current.mutation ||
        improved.appeal > current.appeal;
    return no_worse && strictly_better;
}

bool CoreDominates(
    const snapshot::RoomAttributes& improved,
    const snapshot::RoomAttributes& current) {
    const bool no_worse =
        improved.comfort >= current.comfort &&
        improved.stimulation >= current.stimulation &&
        improved.health >= current.health &&
        improved.mutation >= current.mutation;
    const bool strictly_better =
        improved.comfort > current.comfort ||
        improved.stimulation > current.stimulation ||
        improved.health > current.health ||
        improved.mutation > current.mutation;
    return no_worse && strictly_better;
}

double CoreMinimum(const snapshot::RoomAttributes& attributes) {
    return std::min({
        attributes.comfort,
        attributes.stimulation,
        attributes.health,
        attributes.mutation});
}

double CoreTotal(const snapshot::RoomAttributes& attributes) {
    return attributes.comfort + attributes.stimulation +
        attributes.health + attributes.mutation;
}

snapshot::RoomAttributes Added(
    snapshot::RoomAttributes current,
    const snapshot::RoomAttributes& gain) {
    Add(current, gain);
    return current;
}

double TotalGain(const snapshot::RoomAttributes& gain) {
    return gain.comfort + gain.stimulation + gain.health +
        gain.mutation + gain.appeal;
}

const room_planning::RoomPurposeAssignment* FindPurpose(
    const std::vector<room_planning::RoomPurposeAssignment>& purposes,
    const snapshot::RoomId& room_id) {
    const auto found = std::ranges::find(
        purposes, room_id,
        &room_planning::RoomPurposeAssignment::room_id);
    return found == purposes.end() ? nullptr : &*found;
}

auto PurposeRank(
    room_planning::RoomRole role,
    const snapshot::RoomAttributes& attributes,
    std::size_t residents,
    bool breeding_stats_stable) {
    const auto crowding = residents > 4U ? residents - 4U : 0U;
    const auto effective_comfort =
        attributes.comfort - static_cast<double>(crowding);
    switch (role) {
        case room_planning::RoomRole::Breeding: {
            const auto viable = effective_comfort > -10.0;
            const auto balanced = std::min(
                effective_comfort, attributes.stimulation);
            return std::tuple{
                viable ? 6 : 0,
                balanced,
                effective_comfort,
                attributes.stimulation,
                breeding_stats_stable ? attributes.mutation : 0.0,
                attributes.health};
        }
        case room_planning::RoomRole::CombatStaging:
            // The player explicitly uses the combat room as a controlled
            // low-comfort fight room. Health is the hard safety floor; once
            // non-negative, lower comfort is the primary optimization goal.
            return std::tuple{
                attributes.health >= 0.0 ? 5 : 0,
                -effective_comfort,
                attributes.health,
                attributes.stimulation,
                attributes.mutation,
                0.0};
        case room_planning::RoomRole::Kitten:
        case room_planning::RoomRole::Recovery:
            return std::tuple{
                attributes.health >= 0.0 && effective_comfort >= 0.0 ? 4 : 0,
                attributes.health,
                effective_comfort,
                attributes.stimulation,
                attributes.mutation,
                0.0};
        case room_planning::RoomRole::MutationLab:
            return std::tuple{
                attributes.health >= 0.0 && effective_comfort > -10.0 ? 5 : 0,
                attributes.mutation,
                attributes.health,
                effective_comfort,
                attributes.stimulation,
                0.0};
        default:
            return std::tuple{
                3,
                CoreMinimum(attributes),
                CoreTotal(attributes),
                attributes.health,
                effective_comfort,
                0.0};
    }
}

template<class T>
void Append(std::ostringstream& output, const T& value) {
    output << value << '|';
}

std::string BuildBindingDigest(
    const FurnitureAnalysisSourceSnapshot& source,
    const std::vector<FurnitureAnalysisRoom>& rooms,
    const std::vector<room_planning::RoomPurposeAssignment>& purposes) {
    std::ostringstream canonical;
    Append(canonical, source.house.scene_generation);
    Append(canonical, source.house.game_day.value_or(-1));
    Append(
        canonical,
        workflow::DigestPrivateIdentity(source.house.source_save_name));
    for (const auto& cat : source.house.cats) {
        Append(canonical, cat.id);
        Append(canonical, cat.room_id.value_or(""));
    }
    for (const auto& room : rooms) {
        Append(canonical, room.binding_key);
        Append(canonical, room.resident_count);
        Append(canonical, room.furniture_count);
        Append(canonical, room.attributes.comfort);
        Append(canonical, room.attributes.stimulation);
        Append(canonical, room.attributes.health);
        Append(canonical, room.attributes.mutation);
        Append(canonical, room.attributes.appeal);
    }
    auto ordered_purposes = purposes;
    std::ranges::sort(
        ordered_purposes, {},
        &room_planning::RoomPurposeAssignment::room_id);
    for (const auto& purpose : ordered_purposes) {
        Append(canonical, purpose.room_id);
        Append(canonical, static_cast<int>(purpose.role));
        Append(canonical, purpose.expected_resident_count);
        Append(canonical, purpose.breeding_stats_stable);
    }
    for (const auto& item : source.furniture) {
        Append(canonical, item.instance_id);
        Append(canonical, item.format_version);
        Append(canonical, item.item_id);
        Append(canonical, item.unknown_after_item_length);
        Append(canonical, item.placement_flags);
        Append(canonical, item.room_id);
        Append(canonical, item.unknown_after_room_length);
        Append(canonical, item.position_x);
        Append(canonical, item.position_y);
        Append(canonical, item.position_z);
        Append(canonical, item.scale_x);
        Append(canonical, item.scale_y);
    }
    auto runtime_room_grids = source.runtime_room_grids;
    std::ranges::sort(
        runtime_room_grids,
        [](const auto& left, const auto& right) {
            return std::tie(left.room_id, left.width, left.height) <
                std::tie(right.room_id, right.width, right.height);
        });
    for (const auto& room : runtime_room_grids) {
        Append(canonical, room.room_id);
        Append(canonical, room.width);
        Append(canonical, room.height);
        for (const auto cell : room.base_cells) {
            Append(canonical, static_cast<unsigned int>(cell));
        }
        for (const auto cell : room.live_cells) {
            Append(canonical, static_cast<unsigned int>(cell));
        }
    }
    Append(canonical, source.runtime_scene_piece_count);
    Append(canonical, source.runtime_placed_piece_count);
    auto warehouse_pieces = source.runtime_warehouse_pieces;
    std::ranges::sort(
        warehouse_pieces,
        [](const auto& left, const auto& right) {
            return std::tie(left.stable_key, left.item_id) <
                std::tie(right.stable_key, right.item_id);
        });
    for (const auto& piece : warehouse_pieces) {
        Append(canonical, piece.stable_key);
        Append(canonical, piece.item_id);
    }
    for (const auto& room : source.geometry.rooms) {
        Append(canonical, room.definition_id);
        Append(canonical, room.room_id);
        Append(canonical, room.width);
        Append(canonical, room.height);
        for (const auto& row : room.built_in_collision) {
            for (const auto value : row) {
                Append(canonical, value);
            }
        }
    }
    for (const auto& house : source.geometry.houses) {
        Append(canonical, house.house_id);
        for (const auto& room : house.room_positions) {
            Append(canonical, room.room_definition_id);
            Append(canonical, room.room_id);
            Append(canonical, room.x);
            Append(canonical, room.y);
        }
    }
    for (const auto& item : source.furniture_info.records) {
        Append(canonical, item.item_id);
        Append(canonical, item.unknown_after_name_length);
        for (const auto byte : item.opaque_payload) {
            Append(canonical, std::to_integer<unsigned int>(byte));
        }
    }
    std::vector<std::pair<std::string, snapshot::RoomAttributes>> effects{
        source.furniture_effects.begin(), source.furniture_effects.end()};
    std::ranges::sort(effects, {}, &decltype(effects)::value_type::first);
    for (const auto& [item_id, value] : effects) {
        Append(canonical, item_id);
        Append(canonical, value.comfort);
        Append(canonical, value.stimulation);
        Append(canonical, value.health);
        Append(canonical, value.mutation);
        Append(canonical, value.appeal);
    }
    return workflow::DigestPrivateIdentity(canonical.str());
}

}  // namespace

FurnitureAnalysisService::FurnitureAnalysisService(
    IFurnitureAnalysisSource& source)
    : source_(source) {}

Result<FurnitureAnalysisSnapshot> FurnitureAnalysisService::Analyze(
    std::uint64_t scene_generation,
    const std::vector<snapshot::RoomId>& locked_room_ids,
    const std::vector<std::uint64_t>& blocked_warehouse_keys,
    bool allow_attribute_upgrades,
    const std::vector<room_planning::RoomPurposeAssignment>& room_purposes,
    const std::vector<furniture_planning::FurnitureLayoutStateEdge>&
        forbidden_layout_edges,
    const snapshot::RoomId& preferred_focus_room_id) {
    if (scene_generation == 0U) {
        return {{}, ErrorCode::SceneUnavailable,
                "furniture analysis requires a House generation"};
    }
    auto captured = source_.Capture(scene_generation);
    if (!captured) {
        return {{}, captured.code, captured.message};
    }
    auto& source = captured.value;
    if (source.house.scene_generation != scene_generation) {
        return {{}, ErrorCode::SnapshotInvalid,
                "furniture analysis generation changed during capture"};
    }

    std::map<snapshot::RoomId, FurnitureAnalysisRoom> identified;
    for (const auto& room : source.house.rooms) {
        if (room.id == "AdventureBox") {
            continue;
        }
        auto& analysis = identified[room.id];
        analysis.binding_key = "id:" + room.id;
        analysis.room_id = room.id;
        analysis.resident_count = room.residents.size();
    }
    for (const auto& room_id : source.runtime_detected_room_ids) {
        if (room_id.empty() || room_id == "AdventureBox") {
            continue;
        }
        auto& analysis = identified[room_id];
        analysis.binding_key = "id:" + room_id;
        analysis.room_id = room_id;
    }

    std::unordered_set<std::string> info_ids;
    info_ids.reserve(source.furniture_info.records.size());
    for (const auto& item : source.furniture_info.records) {
        info_ids.insert(item.item_id);
    }

    FurnitureAnalysisSnapshot result;
    result.scene_generation = scene_generation;
    result.game_day = source.house.game_day;
    result.save_identity = workflow::DigestPrivateIdentity(
        source.house.source_save_name);
    result.cat_count = source.house.cats.size();
    result.furniture_count = source.furniture.size();
    result.runtime_scene_piece_count = source.runtime_scene_piece_count;
    result.runtime_placed_piece_count = source.runtime_placed_piece_count;
    result.runtime_warehouse_piece_count =
        source.runtime_warehouse_pieces.size();
    result.room_purposes = room_purposes;
    std::unordered_map<
        std::uint64_t,
        const snapshot::detail::FurniturePlacement*> furniture_by_key;
    furniture_by_key.reserve(source.furniture.size());
    for (const auto& item : source.furniture) {
        if (item.instance_id > 0) {
            furniture_by_key.emplace(
                static_cast<std::uint64_t>(item.instance_id), &item);
        }
    }
    for (const auto& piece : source.runtime_warehouse_pieces) {
        const auto saved = furniture_by_key.find(piece.stable_key);
        if (saved != furniture_by_key.end() &&
            saved->second->room_id.empty() &&
            saved->second->item_id == piece.item_id) {
            ++result.runtime_warehouse_piece_match_count;
        }
    }
    for (const auto& item : source.furniture) {
        result.furniture_info_coverage +=
            info_ids.contains(item.item_id) ? 1U : 0U;
        result.furniture_effect_coverage +=
            source.furniture_effects.contains(item.item_id) ? 1U : 0U;
        if (item.room_id.empty()) {
            ++result.warehouse_furniture_count;
            continue;
        }
        ++result.placed_furniture_count;
        auto& room = identified[item.room_id];
        room.binding_key = "id:" + item.room_id;
        room.room_id = item.room_id;
        ++room.furniture_count;
        const auto effect = source.furniture_effects.find(item.item_id);
        if (effect != source.furniture_effects.end()) {
            Add(room.attributes, effect->second);
        }
    }

    struct UpgradePair {
        const snapshot::detail::FurniturePlacement* placed{};
        const snapshot::detail::FurniturePlacement* warehouse{};
        snapshot::RoomAttributes gain;
        bool balanced_attic{};
        double balanced_minimum{};
        double balanced_total{};
        decltype(PurposeRank(
            room_planning::RoomRole::General,
            snapshot::RoomAttributes{}, 0U, false)) purpose_rank{};
    };
    std::vector<UpgradePair> upgrade_pairs;
    const std::unordered_set<snapshot::RoomId> locked_rooms(
        locked_room_ids.begin(), locked_room_ids.end());
    const std::unordered_set<std::uint64_t> blocked_warehouse(
        blocked_warehouse_keys.begin(), blocked_warehouse_keys.end());
    snapshot::RoomId active_room_id;
    if (!preferred_focus_room_id.empty() &&
        identified.contains(preferred_focus_room_id) &&
        !locked_rooms.contains(preferred_focus_room_id)) {
        active_room_id = preferred_focus_room_id;
    } else {
        for (const auto& [room_id, room] : identified) {
            (void)room;
            if (locked_rooms.contains(room_id)) {
                continue;
            }
            if (active_room_id.empty() ||
                std::tuple{
                    furniture_planning::FurnitureRoomPlacementOrder(room_id),
                    room_id} <
                std::tuple{
                    furniture_planning::FurnitureRoomPlacementOrder(
                        active_room_id),
                    active_room_id}) {
                active_room_id = room_id;
            }
        }
    }
    if (allow_attribute_upgrades) {
        for (const auto& warehouse : source.furniture) {
            if (!warehouse.room_id.empty() || warehouse.instance_id <= 0) {
                continue;
            }
            if (blocked_warehouse.contains(
                    static_cast<std::uint64_t>(warehouse.instance_id))) {
                continue;
            }
            const auto improved =
                source.furniture_effects.find(warehouse.item_id);
            if (improved == source.furniture_effects.end()) {
                continue;
            }
            for (const auto& placed : source.furniture) {
                if (placed.room_id.empty() || placed.instance_id <= 0) {
                    continue;
                }
                if (!active_room_id.empty() &&
                    placed.room_id != active_room_id) {
                    continue;
                }
                if (locked_rooms.contains(placed.room_id)) {
                    continue;
                }
                const auto current =
                    source.furniture_effects.find(placed.item_id);
                if (current == source.furniture_effects.end()) {
                    continue;
                }
                const auto gain = Subtract(
                    improved->second, current->second);
                const auto current_room = identified.find(placed.room_id);
                const auto current_attributes = current_room == identified.end()
                    ? snapshot::RoomAttributes{}
                    : current_room->second.attributes;
                const auto improved_room = current_room == identified.end()
                    ? gain
                    : Added(current_room->second.attributes, gain);
                const auto* purpose = FindPurpose(
                    room_purposes, placed.room_id);
                const bool balanced_attic = !purpose &&
                    placed.room_id == "Attic";
                const auto current_rank = std::tuple{
                    CoreMinimum(current_attributes),
                    CoreTotal(current_attributes),
                    current_attributes.appeal};
                const auto improved_rank = std::tuple{
                    CoreMinimum(improved_room),
                    CoreTotal(improved_room),
                    improved_room.appeal};
                const bool direct_upgrade = balanced_attic
                    ? CoreDominates(improved->second, current->second)
                    : Dominates(improved->second, current->second);
                const auto residents = purpose
                    ? purpose->expected_resident_count
                    : (current_room == identified.end()
                        ? 0U : current_room->second.resident_count);
                const auto role = purpose
                    ? purpose->role : room_planning::RoomRole::General;
                const auto current_purpose_rank = PurposeRank(
                    role,
                    current_attributes,
                    residents,
                    purpose && purpose->breeding_stats_stable);
                const auto improved_purpose_rank = PurposeRank(
                    role,
                    improved_room,
                    residents,
                    purpose && purpose->breeding_stats_stable);
                if (balanced_attic
                        ? !direct_upgrade
                        : (purpose
                            ? improved_purpose_rank <= current_purpose_rank
                            : (!direct_upgrade &&
                               improved_rank <= current_rank))) {
                    continue;
                }
                upgrade_pairs.push_back({
                    .placed = &placed,
                    .warehouse = &warehouse,
                    .gain = gain,
                    .balanced_attic = balanced_attic,
                    .balanced_minimum = CoreMinimum(improved_room),
                    .balanced_total = CoreTotal(improved_room),
                    .purpose_rank = improved_purpose_rank});
            }
        }
    }
    std::ranges::sort(
        upgrade_pairs,
        [](const auto& left, const auto& right) {
            if (left.balanced_attic != right.balanced_attic) {
                return left.balanced_attic;
            }
            if (left.balanced_attic) {
                if (left.balanced_minimum != right.balanced_minimum) {
                    return left.balanced_minimum > right.balanced_minimum;
                }
                if (left.balanced_total != right.balanced_total) {
                    return left.balanced_total > right.balanced_total;
                }
                const auto left_core_gain = CoreTotal(left.gain);
                const auto right_core_gain = CoreTotal(right.gain);
                if (left_core_gain != right_core_gain) {
                    return left_core_gain > right_core_gain;
                }
            }
            if (left.purpose_rank != right.purpose_rank) {
                return left.purpose_rank > right.purpose_rank;
            }
            const auto left_gain = TotalGain(left.gain);
            const auto right_gain = TotalGain(right.gain);
            if (left_gain != right_gain) {
                return left_gain > right_gain;
            }
            return std::tie(
                       left.warehouse->instance_id,
                       left.placed->instance_id) <
                std::tie(
                       right.warehouse->instance_id,
                       right.placed->instance_id);
        });
    std::unordered_set<std::uint64_t> used_placed;
    std::unordered_set<std::uint64_t> used_warehouse;
    auto planning_furniture = source.furniture;
    for (const auto& pair : upgrade_pairs) {
        const auto placed_key =
            static_cast<std::uint64_t>(pair.placed->instance_id);
        const auto warehouse_key =
            static_cast<std::uint64_t>(pair.warehouse->instance_id);
        if (used_placed.contains(placed_key) ||
            used_warehouse.contains(warehouse_key)) {
            continue;
        }
        const auto placed = std::ranges::find_if(
            planning_furniture,
            [placed_key](const auto& item) {
                return item.instance_id ==
                    static_cast<std::int64_t>(placed_key);
            });
        const auto warehouse = std::ranges::find_if(
            planning_furniture,
            [warehouse_key](const auto& item) {
                return item.instance_id ==
                    static_cast<std::int64_t>(warehouse_key);
            });
        if (placed == planning_furniture.end() ||
            warehouse == planning_furniture.end()) {
            return {{}, ErrorCode::SnapshotInvalid,
                    "attribute upgrade identity disappeared before planning"};
        }
        const auto support_dependents =
            furniture_planning::FindFurnitureSupportDependentsTopDown(
                *placed,
                planning_furniture,
                source.geometry,
                source.furniture_info,
                source.runtime_room_grids);
        const auto replacement =
            furniture_planning::FindNearestFurnitureReplacementPlacement(
                *placed,
                *warehouse,
                planning_furniture,
                source.geometry,
                source.furniture_info,
                source.runtime_room_grids);
        if (!support_dependents || !replacement) {
            continue;
        }
        used_placed.insert(placed_key);
        used_warehouse.insert(warehouse_key);
        result.attribute_upgrades.push_back({
            .placed_stable_key = placed_key,
            .warehouse_stable_key = warehouse_key,
            .placed_item_id = pair.placed->item_id,
            .warehouse_item_id = pair.warehouse->item_id,
            .target_room_id = replacement->room_id,
            .original_x = pair.placed->position_x,
            .original_y = pair.placed->position_y,
            .target_x = replacement->x,
            .target_y = replacement->y,
            .support_dependents_top_down = *support_dependents,
            .gain = pair.gain});
        Add(result.attribute_upgrade_gain, pair.gain);
        warehouse->room_id = replacement->room_id;
        warehouse->position_x = replacement->x;
        warehouse->position_y = replacement->y;
        warehouse->position_z = placed->position_z;
        warehouse->scale_x = placed->scale_x;
        warehouse->scale_y = placed->scale_y;
        placed->room_id.clear();

        // The game defers destruction of the replaced FurniturePiece.  A
        // second replacement in the same sealed execution batch can need a
        // stable key whose old component is still pending deletion, and a
        // later failure cannot safely recreate earlier keys during rollback.
        // Seal exactly one native replacement per analysis.  The next player
        // analysis observes the settled scene and selects the next upgrade.
        break;
    }

    // A replaced scene piece is queued for deferred component deletion by
    // the game after it returns to the warehouse. Recreating that same stable
    // key later in this sealed batch races the still-live component and has
    // crashed the current build at Mewgenics.exe+0x5959A. Keep the returned
    // furniture available in the real warehouse, but defer it from layout
    // planning until the player's next analysis captures the settled scene.
    std::erase_if(
        planning_furniture,
        [&used_placed, &blocked_warehouse](const auto& item) {
            if (item.instance_id <= 0) {
                return false;
            }
            const auto key = static_cast<std::uint64_t>(item.instance_id);
            return used_placed.contains(key) || blocked_warehouse.contains(key);
        });

    result.identified_room_count = identified.size();
    result.rooms.reserve(std::max(
        identified.size(), source.available_room_count));
    for (auto& [id, room] : identified) {
        (void)id;
        result.rooms.push_back(std::move(room));
    }
    for (std::size_t index = result.rooms.size();
         index < source.available_room_count;
         ++index) {
        result.rooms.push_back({
            .binding_key = "anonymous-runtime-room:" +
                std::to_string(index - result.identified_room_count + 1U)});
    }
    if (result.rooms.empty()) {
        return {{}, ErrorCode::RoomDataUnavailable,
                "furniture analysis found no current House rooms"};
    }
    result.binding_digest = BuildBindingDigest(
        source, result.rooms, room_purposes);
    std::vector<furniture_planning::FurnitureLayoutMove>
        forbidden_layout_moves;
    for (const auto& edge : forbidden_layout_edges) {
        if (edge.binding_digest == result.binding_digest) {
            forbidden_layout_moves.push_back(edge.first_move);
        }
    }
    // Never mix a native warehouse replacement with layout moves.  A
    // successful replacement finishes this batch; layout planning resumes on
    // the next analysis after the game's deferred component deletion settles.
    if (result.attribute_upgrades.empty()) {
        result.layout_plan = furniture_planning::FurnitureLayoutSolver{}.Plan(
            planning_furniture,
            source.geometry,
            source.furniture_info,
            source.runtime_room_grids,
            locked_room_ids,
            source.furniture_effects,
            room_purposes,
            forbidden_layout_moves,
            active_room_id);
    } else {
        result.layout_plan.target_room_id = active_room_id;
        result.layout_plan.planned_room_count = active_room_id.empty() ? 0U : 1U;
    }
    return {std::move(result)};
}

}  // namespace autocattery::furniture_analysis
