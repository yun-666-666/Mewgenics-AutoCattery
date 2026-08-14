#include "auto_cattery/furniture_planning/layout_solver.hpp"
#include "auto_cattery/furniture_planning/purpose_policy.hpp"

#include "test_support.hpp"

#include <algorithm>
#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace autocattery::tests {
namespace {

using snapshot::detail::FurniturePlacementTile;

snapshot::detail::FurnitureInfoRecord AnchoredInfo(
    std::string item,
    std::size_t width) {
    snapshot::detail::FurnitureInfoRecord info;
    info.item_id = std::move(item);
    info.placement_grid.supported = true;
    for (std::size_t x = 0; x < width; ++x) {
        info.placement_grid.tiles[
            9U * snapshot::detail::kFurniturePlacementGridWidth +
            10U + x] = FurniturePlacementTile::Support;
        info.placement_grid.tiles[
            10U * snapshot::detail::kFurniturePlacementGridWidth +
            10U + x] = FurniturePlacementTile::Hitbox;
        info.placement_grid.tiles[
            11U * snapshot::detail::kFurniturePlacementGridWidth +
            10U + x] = FurniturePlacementTile::Solid;
    }
    return info;
}

snapshot::detail::FurnitureInfoRecord SmallInfo(std::string item) {
    snapshot::detail::FurnitureInfoRecord info;
    info.item_id = std::move(item);
    info.placement_grid.supported = true;
    info.placement_grid.tiles[
        11U * snapshot::detail::kFurniturePlacementGridWidth + 10U] =
        FurniturePlacementTile::Support;
    info.placement_grid.tiles[
        12U * snapshot::detail::kFurniturePlacementGridWidth + 10U] =
        FurniturePlacementTile::Hitbox;
    return info;
}

snapshot::detail::FurnitureInfoRecord HangingInfo(std::string item) {
    snapshot::detail::FurnitureInfoRecord info;
    info.item_id = std::move(item);
    info.placement_grid.supported = true;
    info.placement_grid.tiles[
        9U * snapshot::detail::kFurniturePlacementGridWidth + 11U] =
        FurniturePlacementTile::Solid;
    info.placement_grid.tiles[
        11U * snapshot::detail::kFurniturePlacementGridWidth + 11U] =
        FurniturePlacementTile::Solid;
    info.placement_grid.tiles[
        14U * snapshot::detail::kFurniturePlacementGridWidth + 10U] =
        FurniturePlacementTile::Support;
    return info;
}

snapshot::detail::FurnitureInfoRecord PosterInfo(std::string item) {
    snapshot::detail::FurnitureInfoRecord info;
    info.item_id = std::move(item);
    info.placement_grid.supported = true;
    info.placement_grid.tiles[
        12U * snapshot::detail::kFurniturePlacementGridWidth + 10U] =
        FurniturePlacementTile::Hitbox;
    return info;
}

snapshot::detail::FurnitureInfoRecord WidePosterInfo(std::string item) {
    auto info = PosterInfo(std::move(item));
    info.placement_grid.tiles[
        12U * snapshot::detail::kFurniturePlacementGridWidth + 11U] =
        FurniturePlacementTile::Hitbox;
    return info;
}

snapshot::detail::FurnitureInfoRecord CouchInfo(std::string item) {
    snapshot::detail::FurnitureInfoRecord info;
    info.item_id = std::move(item);
    info.placement_grid.supported = true;
    for (const auto x : {9U, 10U, 13U, 14U}) {
        info.placement_grid.tiles[
            9U * snapshot::detail::kFurniturePlacementGridWidth + x] =
            FurniturePlacementTile::Support;
    }
    for (const auto x : {9U, 14U}) {
        info.placement_grid.tiles[
            10U * snapshot::detail::kFurniturePlacementGridWidth + x] =
            FurniturePlacementTile::Hitbox;
        info.placement_grid.tiles[
            11U * snapshot::detail::kFurniturePlacementGridWidth + x] =
            FurniturePlacementTile::Hitbox;
    }
    for (std::size_t x = 10U; x <= 13U; ++x) {
        info.placement_grid.tiles[
            10U * snapshot::detail::kFurniturePlacementGridWidth + x] =
            FurniturePlacementTile::Solid;
        info.placement_grid.tiles[
            11U * snapshot::detail::kFurniturePlacementGridWidth + x] =
            FurniturePlacementTile::Surface;
    }
    return info;
}

snapshot::detail::FurnitureInfoRecord DresserInfo(std::string item) {
    snapshot::detail::FurnitureInfoRecord info;
    info.item_id = std::move(item);
    info.placement_grid.supported = true;
    for (const auto x : {11U, 12U}) {
        info.placement_grid.tiles[
            9U * snapshot::detail::kFurniturePlacementGridWidth + x] =
            FurniturePlacementTile::Support;
        info.placement_grid.tiles[
            10U * snapshot::detail::kFurniturePlacementGridWidth + x] =
            FurniturePlacementTile::Hitbox;
        info.placement_grid.tiles[
            11U * snapshot::detail::kFurniturePlacementGridWidth + x] =
            FurniturePlacementTile::Hitbox;
    }
    for (std::size_t x = 10U; x <= 13U; ++x) {
        info.placement_grid.tiles[
            12U * snapshot::detail::kFurniturePlacementGridWidth + x] =
            FurniturePlacementTile::Solid;
    }
    return info;
}

snapshot::detail::FurnitureInfoRecord BoneSinkInfo(std::string item) {
    snapshot::detail::FurnitureInfoRecord info;
    info.item_id = std::move(item);
    info.placement_grid.supported = true;
    for (std::size_t x = 10U; x <= 12U; ++x) {
        info.placement_grid.tiles[
            9U * snapshot::detail::kFurniturePlacementGridWidth + x] =
            FurniturePlacementTile::Support;
        info.placement_grid.tiles[
            10U * snapshot::detail::kFurniturePlacementGridWidth + x] =
            FurniturePlacementTile::Hitbox;
        info.placement_grid.tiles[
            11U * snapshot::detail::kFurniturePlacementGridWidth + x] =
            FurniturePlacementTile::Solid;
    }
    for (const auto x : {11U, 12U}) {
        info.placement_grid.tiles[
            12U * snapshot::detail::kFurniturePlacementGridWidth + x] =
            FurniturePlacementTile::Surface;
    }
    return info;
}

snapshot::detail::FurniturePlacement Placement(
    std::int64_t key,
    std::string item,
    std::string room,
    std::int32_t x,
    std::int32_t y) {
    return {
        .instance_id = key,
        .item_id = std::move(item),
        .room_id = std::move(room),
        .position_x = x,
        .position_y = y,
        .scale_x = 1,
        .scale_y = 1};
}

using Position = std::pair<std::int32_t, std::int32_t>;

struct PlacementState {
    std::string room;
    Position position;

    bool operator==(const PlacementState&) const = default;
};

std::map<std::uint64_t, Position> FinalPositions(
    const std::vector<snapshot::detail::FurniturePlacement>& furniture,
    const furniture_planning::FurnitureLayoutPlan& plan) {
    std::map<std::uint64_t, Position> positions;
    for (const auto& item : furniture) {
        if (item.instance_id > 0) {
            positions[static_cast<std::uint64_t>(item.instance_id)] = {
                item.position_x, item.position_y};
        }
    }
    for (const auto& move : plan.moves) {
        positions[move.stable_key] = {move.target_x, move.target_y};
    }
    return positions;
}

std::map<std::uint64_t, PlacementState> FinalPlacementStates(
    const std::vector<snapshot::detail::FurniturePlacement>& furniture,
    const furniture_planning::FurnitureLayoutPlan& plan) {
    std::map<std::uint64_t, PlacementState> states;
    for (const auto& item : furniture) {
        if (item.instance_id > 0) {
            states[static_cast<std::uint64_t>(item.instance_id)] = {
                item.room_id, {item.position_x, item.position_y}};
        }
    }
    for (const auto& move : plan.moves) {
        const auto current = states.find(move.stable_key);
        if (current == states.end()) {
            throw std::runtime_error(
                "layout move stable key missing from input furniture: " +
                std::to_string(move.stable_key));
            continue;
        }
        AC_CHECK(current->second.room == move.from_room_id);
        AC_CHECK(current->second.position ==
            Position(move.from_x, move.from_y));
        current->second = {
            move.target_room_id,
            {move.target_x, move.target_y}};
    }
    return states;
}

}  // namespace

void RunFurnitureLayoutSolverTests() {
    AC_CHECK(furniture_planning::FurnitureRoomPlacementOrder("Attic") <
        furniture_planning::FurnitureRoomPlacementOrder("Floor2_Large"));
    AC_CHECK(furniture_planning::FurnitureRoomPlacementOrder("Floor2_Large") <
        furniture_planning::FurnitureRoomPlacementOrder("Floor1_Large"));
    AC_CHECK(furniture_planning::FurnitureRoomPlacementOrder("Floor1_Large") <
        furniture_planning::FurnitureRoomPlacementOrder("Floor1_Small"));
    AC_CHECK(furniture_planning::FurnitureRoomPlacementOrder("Floor1_Small") <
        furniture_planning::FurnitureRoomPlacementOrder("Floor2_Small"));
    const furniture_planning::FurnitureLayoutMove move_down{
        90U, "wallmounted_cloud", "Floor1_Small", "Floor1_Small",
        -10, -8, -10, -9};
    const furniture_planning::FurnitureLayoutMove move_up{
        90U, "wallmounted_cloud", "Floor1_Small", "Floor1_Small",
        -10, -9, -10, -8};
    AC_CHECK(furniture_planning::IsImmediateReverseLayoutMove(
        move_down, move_up));
    auto different_key = move_up;
    different_key.stable_key = 91U;
    AC_CHECK(!furniture_planning::IsImmediateReverseLayoutMove(
        move_down, different_key));

    using EdgeStatus =
        furniture_planning::FurnitureLayoutStateEdgeRecordStatus;
    std::vector<furniture_planning::FurnitureLayoutStateEdge>
        attempted_state_edges;
    AC_CHECK(furniture_planning::RecordFurnitureLayoutStateEdge(
        attempted_state_edges, "binding-A", move_down) ==
        EdgeStatus::Recorded);
    AC_CHECK(furniture_planning::RecordFurnitureLayoutStateEdge(
        attempted_state_edges, "binding-A", move_down) ==
        EdgeStatus::Duplicate);
    AC_CHECK(furniture_planning::RecordFurnitureLayoutStateEdge(
        attempted_state_edges, "binding-A", move_up) ==
        EdgeStatus::Recorded);
    AC_CHECK(furniture_planning::RecordFurnitureLayoutStateEdge(
        attempted_state_edges, "binding-B", move_down) ==
        EdgeStatus::Recorded);

    std::vector<furniture_planning::FurnitureLayoutStateEdge>
        five_state_cycle;
    const std::vector<std::string> cycle_bindings{
        "state-A", "state-B", "state-C", "state-D", "state-E"};
    for (std::size_t index = 0; index < cycle_bindings.size(); ++index) {
        auto cycle_move = move_down;
        cycle_move.stable_key = 100U + index;
        cycle_move.item_id = "cycle-item-" + std::to_string(index);
        cycle_move.from_x = static_cast<std::int32_t>(index);
        cycle_move.target_x = static_cast<std::int32_t>(index + 1U);
        AC_CHECK(furniture_planning::RecordFurnitureLayoutStateEdge(
            five_state_cycle, cycle_bindings[index], cycle_move) ==
            EdgeStatus::Recorded);
    }
    auto repeated_cycle_move = move_down;
    repeated_cycle_move.stable_key = 100U;
    repeated_cycle_move.item_id = "cycle-item-0";
    repeated_cycle_move.from_x = 0;
    repeated_cycle_move.target_x = 1;
    AC_CHECK(furniture_planning::RecordFurnitureLayoutStateEdge(
        five_state_cycle, "state-A", repeated_cycle_move) ==
        EdgeStatus::Duplicate);
    auto different_furniture_from_state_a = repeated_cycle_move;
    different_furniture_from_state_a.stable_key = 205U;
    different_furniture_from_state_a.item_id = "other-furniture";
    AC_CHECK(furniture_planning::RecordFurnitureLayoutStateEdge(
        five_state_cycle,
        "state-A",
        different_furniture_from_state_a) == EdgeStatus::Recorded);

    std::vector<furniture_planning::FurnitureLayoutStateEdge>
        bounded_state_edges;
    AC_CHECK(furniture_planning::RecordFurnitureLayoutStateEdge(
        bounded_state_edges, "only-state", move_down, 1U) ==
        EdgeStatus::Recorded);
    AC_CHECK(furniture_planning::RecordFurnitureLayoutStateEdge(
        bounded_state_edges, "new-state", move_up, 1U) ==
        EdgeStatus::CapacityReached);

    snapshot::detail::HouseGeometryCatalog geometry;
    geometry.rooms.push_back({
        .definition_id = "R1",
        .room_id = "RoomA",
        .width = 6,
        .height = 4});
    snapshot::detail::FurnitureInfoCatalog info;
    info.records.push_back(AnchoredInfo("large", 2));
    info.records.push_back(AnchoredInfo("base", 1));
    info.records.push_back(SmallInfo("small"));
    info.records.push_back(PosterInfo("poster"));

    snapshot::detail::FurnitureInfoCatalog replacement_info;
    replacement_info.records.push_back(PosterInfo("old"));
    replacement_info.records.push_back(PosterInfo("blocker"));
    replacement_info.records.push_back(WidePosterInfo("replacement"));
    const auto old = Placement(1, "old", "RoomA", -10, -12);
    const auto blocker = Placement(2, "blocker", "RoomA", -9, -12);
    const auto warehouse = Placement(3, "replacement", "", 0, 0);
    const std::vector<snapshot::detail::FurniturePlacement>
        replacement_furniture{old, blocker, warehouse};
    std::vector<std::uint8_t> replacement_base(12U, 0U);
    auto replacement_live = replacement_base;
    replacement_live[0] = 1U;
    replacement_live[1] = 1U;
    const auto replacement_target =
        furniture_planning::FindNearestFurnitureReplacementPlacement(
            old,
            warehouse,
            replacement_furniture,
            geometry,
            replacement_info,
            {{"RoomA", 4, 3, replacement_base, replacement_live}});
    AC_CHECK(replacement_target.has_value());
    AC_CHECK(replacement_target->room_id == "RoomA");
    AC_CHECK(replacement_target->x == -10);
    AC_CHECK(replacement_target->y == -11);

    const std::vector<snapshot::detail::FurniturePlacement> first_layout{
        Placement(10, "large", "RoomA", -6, -9),
        Placement(20, "small", "RoomA", -6, -9),
        Placement(30, "base", "RoomA", -9, -9),
        Placement(40, "poster", "RoomA", -5, -8),
        Placement(50, "small", "", 0, 0)};
    const std::vector<snapshot::detail::FurniturePlacement> second_layout{
        Placement(10, "large", "RoomA", -9, -9),
        Placement(20, "small", "RoomA", -8, -9),
        Placement(30, "base", "RoomA", -5, -9),
        Placement(40, "poster", "RoomA", -5, -8),
        Placement(50, "small", "", 0, 0)};

    const furniture_planning::FurnitureLayoutSolver solver;
    const auto first = solver.Plan(first_layout, geometry, info);
    const auto second = solver.Plan(second_layout, geometry, info);
    AC_CHECK(first.planned_room_count == 1);
    AC_CHECK(first.considered_furniture_count == 4);
    AC_CHECK(first.warehouse_furniture_count == 1);
    AC_CHECK(first.unsupported_furniture_count == 0);
    AC_CHECK(first.no_space_furniture_count == 0);
    AC_CHECK(second.unsupported_furniture_count == 0);
    AC_CHECK(second.no_space_furniture_count == 0);
    AC_CHECK(
        FinalPositions(first_layout, first) ==
        FinalPositions(second_layout, second));

    const auto packed = FinalPositions(first_layout, first);
    AC_CHECK(packed.contains(10));
    AC_CHECK(packed.contains(20));
    AC_CHECK(packed.contains(30));
    AC_CHECK(packed.contains(40));

    const std::vector<snapshot::detail::FurniturePlacement> stacked_layout{
        Placement(60, "large", "RoomA", -6, -9),
        Placement(70, "small", "RoomA", -6, -9)};
    const auto stacked = solver.Plan(stacked_layout, geometry, info);
    AC_CHECK(stacked.unsupported_furniture_count == 0);
    AC_CHECK(stacked.no_space_furniture_count == 0);
    AC_CHECK(stacked.moves.size() == 3);
    if (stacked.moves.size() == 3) {
        AC_CHECK(stacked.moves[0].stable_key == 70);
        AC_CHECK(stacked.moves[1].stable_key == 60);
        AC_CHECK(stacked.moves[2].stable_key == 70);
        AC_CHECK(stacked.moves[1].from_x == -6);
        AC_CHECK(stacked.moves[2].from_x == stacked.moves[0].target_x);
    }

    snapshot::detail::FurnitureInfoCatalog dependency_info;
    dependency_info.records = {
        AnchoredInfo("dependency-base", 1),
        AnchoredInfo("dependency-middle", 1),
        AnchoredInfo("dependency-top", 1)};
    const std::vector<snapshot::detail::FurniturePlacement>
        dependency_layout{
            Placement(601, "dependency-base", "RoomA", -6, -9),
            Placement(602, "dependency-middle", "RoomA", -6, -7),
            Placement(603, "dependency-top", "RoomA", -6, -5)};
    const std::vector<furniture_planning::FurnitureRoomGrid>
        dependency_grids{{
            "RoomA",
            6,
            8,
            std::vector<std::uint8_t>(48U, 0U),
            std::vector<std::uint8_t>(48U, 0U)}};
    const auto support_dependents =
        furniture_planning::FindFurnitureSupportDependentsTopDown(
            dependency_layout.front(),
            dependency_layout,
            geometry,
            dependency_info,
            dependency_grids);
    AC_CHECK(support_dependents.has_value());
    AC_CHECK(support_dependents->size() == 2);
    if (support_dependents->size() == 2) {
        AC_CHECK((*support_dependents)[0].stable_key == 603);
        AC_CHECK((*support_dependents)[1].stable_key == 602);
        AC_CHECK((*support_dependents)[0].x == -6);
        AC_CHECK((*support_dependents)[0].y == -5);
    }

    const std::vector<snapshot::detail::FurniturePlacement>
        player_compact_layout{
            Placement(71, "large", "RoomA", -6, -9),
            Placement(72, "small", "RoomA", -6, -9),
            Placement(73, "small", "RoomA", -5, -9)};
    const auto player_compact = solver.Plan(
        player_compact_layout, geometry, info);
    AC_CHECK(player_compact.moves.empty());
    AC_CHECK(player_compact.kept_furniture_count == 3);
    AC_CHECK(player_compact.unsupported_furniture_count == 0);
    AC_CHECK(player_compact.installation_blocked_room_count == 0);

    const std::vector<snapshot::detail::FurniturePlacement>
        overlapping_current_layout{
            Placement(80, "large", "RoomA", -6, -9),
            Placement(90, "base", "RoomA", -6, -9),
            Placement(100, "small", "RoomA", -6, -9)};
    const auto overlapping = solver.Plan(
        overlapping_current_layout, geometry, info);
    AC_CHECK(overlapping.planned_room_count == 1);
    AC_CHECK(overlapping.considered_furniture_count == 3);
    AC_CHECK(overlapping.unsupported_furniture_count == 0);
    AC_CHECK(overlapping.no_space_furniture_count == 0);
    AC_CHECK(!overlapping.moves.empty());

    const std::vector<snapshot::detail::FurniturePlacement>
        floating_current_layout{
            Placement(110, "large", "RoomA", -6, -9),
            Placement(120, "small", "RoomA", -4, -8)};
    const auto floating = solver.Plan(
        floating_current_layout, geometry, info);
    AC_CHECK(floating.planned_room_count == 1);
    AC_CHECK(floating.considered_furniture_count == 2);
    AC_CHECK(floating.unsupported_furniture_count == 0);
    AC_CHECK(floating.no_space_furniture_count == 0);

    auto surface_info = AnchoredInfo("surface_base", 1);
    surface_info.placement_grid.tiles[
        10U * snapshot::detail::kFurniturePlacementGridWidth + 11U] =
        FurniturePlacementTile::Surface;
    auto metadata_info = PosterInfo("metadata_only");
    metadata_info.placement_grid.tiles[
        11U * snapshot::detail::kFurniturePlacementGridWidth + 10U] =
        FurniturePlacementTile::Surface;
    metadata_info.placement_grid.tiles[
        11U * snapshot::detail::kFurniturePlacementGridWidth + 11U] =
        FurniturePlacementTile::PoopLogic;
    auto info_with_metadata = info;
    info_with_metadata.records.push_back(std::move(surface_info));
    info_with_metadata.records.push_back(std::move(metadata_info));
    const std::vector<snapshot::detail::FurniturePlacement> metadata_layout{
        Placement(130, "surface_base", "RoomA", -6, -9),
        Placement(140, "metadata_only", "RoomA", -5, -8)};
    const auto metadata = solver.Plan(
        metadata_layout, geometry, info_with_metadata);
    AC_CHECK(metadata.planned_room_count == 1);
    AC_CHECK(metadata.considered_furniture_count == 2);
    AC_CHECK(metadata.unsupported_furniture_count == 0);
    AC_CHECK(metadata.no_space_furniture_count == 0);

    auto two_room_geometry = geometry;
    two_room_geometry.rooms.push_back({
        .definition_id = "R2",
        .room_id = "RoomB",
        .width = 6,
        .height = 4});
    const std::vector<snapshot::detail::FurniturePlacement> two_rooms{
        Placement(150, "large", "RoomA", -6, -9),
        Placement(160, "small", "RoomA", -6, -9),
        Placement(170, "large", "RoomB", -6, -9),
        Placement(180, "small", "RoomB", -6, -9)};
    const auto one_room_at_a_time = solver.Plan(
        two_rooms, two_room_geometry, info);
    AC_CHECK(one_room_at_a_time.planned_room_count == 1);
    AC_CHECK(!one_room_at_a_time.moves.empty());
    AC_CHECK(std::ranges::all_of(
        one_room_at_a_time.moves,
        [&one_room_at_a_time](const auto& move) {
            return move.from_room_id ==
                    one_room_at_a_time.moves.front().from_room_id &&
                move.target_room_id ==
                    one_room_at_a_time.moves.front().target_room_id;
        }));

    snapshot::detail::HouseGeometryCatalog overlap_geometry;
    overlap_geometry.rooms.push_back({
        .definition_id = "OA",
        .room_id = "SourceA",
        .width = 6,
        .height = 5});
    overlap_geometry.rooms.push_back({
        .definition_id = "OB",
        .room_id = "SourceB",
        .width = 6,
        .height = 5});
    overlap_geometry.rooms.push_back({
        .definition_id = "OT",
        .room_id = "Attic",
        .width = 6,
        .height = 5});
    snapshot::detail::FurnitureInfoCatalog overlap_info;
    overlap_info.records.push_back(CouchInfo("couch"));
    overlap_info.records.push_back(DresserInfo("dresser"));
    const std::vector<furniture_planning::FurnitureRoomGrid> overlap_grids{
        {"SourceA", 8, 7},
        {"SourceB", 8, 7},
        {"Attic", 8, 7}};
    const std::vector<snapshot::detail::FurniturePlacement>
        overlap_furniture{
            Placement(190, "couch", "SourceA", -8, -9),
            Placement(191, "dresser", "SourceB", -8, -9)};
    const auto overlap_plan = solver.Plan(
        overlap_furniture,
        overlap_geometry,
        overlap_info,
        overlap_grids);
    AC_CHECK(overlap_plan.target_room_id == "Attic");
    AC_CHECK(overlap_plan.deferred_furniture_count == 0);
    const auto overlap_final = FinalPlacementStates(
        overlap_furniture, overlap_plan);
    AC_CHECK(overlap_final.at(190).room == "Attic");
    AC_CHECK(overlap_final.at(191).room == "Attic");
    AC_CHECK(overlap_final.at(191).position.second ==
        overlap_final.at(190).position.second + 1);

    const std::vector<snapshot::detail::FurniturePlacement>
        warehouse_fill_furniture{
            Placement(212, "couch", "Attic", -8, -9),
            Placement(213, "dresser", "", 0, 0)};
    const auto warehouse_fill_plan = solver.Plan(
        warehouse_fill_furniture,
        overlap_geometry,
        overlap_info,
        overlap_grids,
        {"SourceA", "SourceB"});
    AC_CHECK(warehouse_fill_plan.target_room_id == "Attic");
    AC_CHECK(warehouse_fill_plan.warehouse_furniture_count == 1);
    AC_CHECK(warehouse_fill_plan.evacuation_blocked_room_count == 0);
    const auto warehouse_fill_move = std::ranges::find_if(
        warehouse_fill_plan.moves,
        [](const auto& move) {
            return move.stable_key == 213U;
        });
    AC_CHECK(warehouse_fill_move != warehouse_fill_plan.moves.end());
    if (warehouse_fill_move != warehouse_fill_plan.moves.end()) {
        AC_CHECK(furniture_planning::IsWarehouseLayoutMove(
            *warehouse_fill_move));
        AC_CHECK(warehouse_fill_move->target_room_id == "Attic");
    }
    const auto warehouse_fill_final = FinalPlacementStates(
        warehouse_fill_furniture, warehouse_fill_plan);
    AC_CHECK(warehouse_fill_final.at(212).room == "Attic");
    AC_CHECK(warehouse_fill_final.at(213).room == "Attic");

    snapshot::detail::HouseGeometryCatalog compact_attic_geometry;
    snapshot::detail::HouseGeometryCatalog balanced_attic_geometry;
    balanced_attic_geometry.rooms.push_back({
        .definition_id = "BalancedAttic",
        .room_id = "Attic",
        .width = 2,
        .height = 1});
    snapshot::detail::FurnitureInfoCatalog balanced_attic_info;
    balanced_attic_info.records = {
        PosterInfo("attic-current"),
        PosterInfo("health-for-stimulation"),
        PosterInfo("balanced-core")};
    const std::vector<snapshot::detail::FurniturePlacement>
        balanced_attic_furniture{
            Placement(214, "attic-current", "Attic", -10, -12),
            Placement(215, "health-for-stimulation", "", 0, 0),
            Placement(216, "balanced-core", "", 0, 0)};
    const snapshot::detail::FurnitureCatalog balanced_attic_effects{
        {"attic-current", snapshot::RoomAttributes{
            .comfort = 5, .stimulation = 44, .health = -7,
            .mutation = 8}},
        {"health-for-stimulation", snapshot::RoomAttributes{
            .comfort = 20, .stimulation = -20, .health = 20,
            .mutation = 20}},
        {"balanced-core", snapshot::RoomAttributes{
            .comfort = 4, .stimulation = 4, .health = 4,
            .mutation = 4}}};
    const auto balanced_attic_plan = solver.Plan(
        balanced_attic_furniture,
        balanced_attic_geometry,
        balanced_attic_info,
        {{"Attic", 2, 1, {0U, 0U}, {1U, 0U}}},
        {},
        balanced_attic_effects);
    AC_CHECK(balanced_attic_plan.target_room_id == "Attic");
    AC_CHECK(std::ranges::any_of(
        balanced_attic_plan.moves,
        [](const auto& move) { return move.stable_key == 216U; }));
    AC_CHECK(std::ranges::none_of(
        balanced_attic_plan.moves,
        [](const auto& move) { return move.stable_key == 215U; }));

    snapshot::detail::FurnitureInfoCatalog attribute_priority_info;
    attribute_priority_info.records = {
        SmallInfo("core-health"),
        SmallInfo("appeal-a"),
        SmallInfo("appeal-b")};
    const std::vector<snapshot::detail::FurniturePlacement>
        attribute_priority_furniture{
            Placement(222, "core-health", "SourceA", -8, -9),
            Placement(223, "appeal-a", "", 0, 0),
            Placement(224, "appeal-b", "", 0, 0)};
    const snapshot::detail::FurnitureCatalog attribute_priority_effects{
        {"core-health", snapshot::RoomAttributes{.health = 5}},
        {"appeal-a", snapshot::RoomAttributes{.appeal = 4}},
        {"appeal-b", snapshot::RoomAttributes{.appeal = 4}}};
    const auto attribute_priority_plan = solver.Plan(
        attribute_priority_furniture,
        overlap_geometry,
        attribute_priority_info,
        {{"Attic", 8, 7},
        {"SourceA", 8, 7}},
        {},
        attribute_priority_effects);
    AC_CHECK(attribute_priority_plan.target_room_id == "Attic");
    AC_CHECK(std::ranges::any_of(
        attribute_priority_plan.moves,
        [](const auto& move) { return move.stable_key == 222U; }));

    const std::vector<snapshot::detail::FurniturePlacement>
        negative_attic_furniture{
            Placement(225, "attic-current", "Attic", -10, -12),
            Placement(226, "special_fightidol", "", 0, 0),
            Placement(227, "balanced-core", "", 0, 0)};
    auto negative_attic_info = balanced_attic_info;
    negative_attic_info.records.push_back(
        PosterInfo("special_fightidol"));
    auto negative_attic_effects = balanced_attic_effects;
    negative_attic_effects["special_fightidol"] =
        snapshot::RoomAttributes{.comfort = -5};
    const auto negative_attic_plan = solver.Plan(
        negative_attic_furniture,
        balanced_attic_geometry,
        negative_attic_info,
        {{"Attic", 2, 1, {0U, 0U}, {1U, 0U}}},
        {},
        negative_attic_effects);
    AC_CHECK(std::ranges::none_of(
        negative_attic_plan.moves,
        [](const auto& move) { return move.stable_key == 226U; }));

    const std::vector<room_planning::RoomPurposeAssignment>
        breeding_attic_purpose{{
            .room_id = "Attic",
            .role = room_planning::RoomRole::Breeding,
            .expected_resident_count = 4,
            .breeding_stats_stable = false}};
    const auto purpose_attic_plan = solver.Plan(
        balanced_attic_furniture,
        balanced_attic_geometry,
        balanced_attic_info,
        {{"Attic", 2, 1, {0U, 0U}, {1U, 0U}}},
        {},
        balanced_attic_effects,
        breeding_attic_purpose);
    AC_CHECK(purpose_attic_plan.target_room_id == "Attic");
    AC_CHECK(std::ranges::any_of(
        purpose_attic_plan.moves,
        [](const auto& move) { return move.stable_key == 215U; }));
    AC_CHECK(std::ranges::none_of(
        purpose_attic_plan.moves,
        [](const auto& move) { return move.stable_key == 216U; }));

    snapshot::detail::HouseGeometryCatalog truncated_source_geometry;
    truncated_source_geometry.rooms = {
        {.definition_id = "TruncatedSource",
         .room_id = "SourceA", .width = 6, .height = 5},
        {.definition_id = "TruncatedAttic",
         .room_id = "Attic", .width = 6, .height = 5}};
    snapshot::detail::FurnitureInfoCatalog truncated_source_info;
    std::vector<snapshot::detail::FurniturePlacement>
        truncated_source_furniture;
    for (std::int64_t index = 0; index < 30; ++index) {
        const auto item_id = "source-base-" + std::to_string(index);
        truncated_source_info.records.push_back(
            AnchoredInfo(item_id, 1));
        truncated_source_furniture.push_back(
            Placement(500 + index, item_id, "SourceA", -8, -9));
    }
    truncated_source_info.records.push_back(
        SmallInfo("source-dependent"));
    truncated_source_info.records.push_back(
        SmallInfo("special_stimulationidol"));
    truncated_source_furniture.push_back(
        Placement(590, "source-dependent", "SourceA", -8, -9));
    truncated_source_furniture.push_back(
        Placement(591, "special_stimulationidol", "SourceA", -7, -11));
    const snapshot::detail::FurnitureCatalog truncated_source_effects{
        {"source-dependent", snapshot::RoomAttributes{.appeal = 1}},
        {"special_stimulationidol",
         snapshot::RoomAttributes{.stimulation = 5}}};
    const auto truncated_source_plan = solver.Plan(
        truncated_source_furniture,
        truncated_source_geometry,
        truncated_source_info,
        {{"SourceA", 8, 7}, {"Attic", 8, 7}},
        {},
        truncated_source_effects);
    AC_CHECK(truncated_source_plan.target_room_id == "Attic");
    AC_CHECK(truncated_source_plan.evacuation_blocked_room_count == 0);
    AC_CHECK(std::ranges::any_of(
        truncated_source_plan.moves,
        [](const auto& move) {
            return move.stable_key == 591U &&
                move.from_room_id == "SourceA" &&
                move.target_room_id == "Attic";
        }));

    snapshot::detail::FurnitureInfoCatalog surface_support_info;
    surface_support_info.records.push_back(BoneSinkInfo("bone_sink"));
    surface_support_info.records.push_back(AnchoredInfo("cinderblock", 2));
    const std::vector<snapshot::detail::FurniturePlacement>
        surface_support_furniture{
            Placement(192, "bone_sink", "SourceA", -8, -9),
            Placement(193, "cinderblock", "SourceB", -8, -9)};
    const auto surface_support_plan = solver.Plan(
        surface_support_furniture,
        overlap_geometry,
        surface_support_info,
        overlap_grids);
    AC_CHECK(surface_support_plan.target_room_id == "Attic");
    AC_CHECK(surface_support_plan.deferred_furniture_count == 0);
    const auto surface_support_final = FinalPlacementStates(
        surface_support_furniture, surface_support_plan);
    AC_CHECK(surface_support_final.at(192).room == "Attic");
    AC_CHECK(surface_support_final.at(193).room == "Attic");
    AC_CHECK(!(
        surface_support_final.at(193).position.first ==
            surface_support_final.at(192).position.first + 1 &&
        surface_support_final.at(193).position.second ==
            surface_support_final.at(192).position.second + 1));

    snapshot::detail::FurnitureInfoCatalog exact_runtime_info;
    exact_runtime_info.records.push_back(AnchoredInfo("exact_mobile", 1));
    exact_runtime_info.records.push_back(PosterInfo("fixed_hitbox"));
    std::vector<std::uint8_t> exact_attic_base(8U * 7U, 0U);
    exact_attic_base[4U] = 2U;
    exact_attic_base[5U] = 2U;
    auto exact_attic_live = exact_attic_base;
    exact_attic_live[1U * 8U + 4U] = 1U;
    const std::vector<furniture_planning::FurnitureRoomGrid>
        exact_runtime_grids{
            {"SourceA", 8, 7},
            {"Attic", 8, 7, exact_attic_base, exact_attic_live}};
    const std::vector<snapshot::detail::FurniturePlacement>
        exact_runtime_furniture{
            Placement(194, "exact_mobile", "SourceA", -8, -9),
            Placement(195, "fixed_hitbox", "Attic", -6, -11)};
    const auto exact_runtime_plan = solver.Plan(
        exact_runtime_furniture,
        overlap_geometry,
        exact_runtime_info,
        exact_runtime_grids);
    AC_CHECK(exact_runtime_plan.target_room_id == "Attic");
    AC_CHECK(exact_runtime_plan.unsupported_furniture_count == 0);
    const auto exact_mobile_move = std::ranges::find_if(
        exact_runtime_plan.moves,
        [](const auto& move) { return move.stable_key == 194U; });
    AC_CHECK(exact_mobile_move != exact_runtime_plan.moves.end());
    if (exact_mobile_move != exact_runtime_plan.moves.end()) {
        AC_CHECK(exact_mobile_move->target_room_id == "Attic");
        AC_CHECK(!(
            exact_mobile_move->target_x == -6 &&
            exact_mobile_move->target_y == -11));
    }
    AC_CHECK(std::ranges::any_of(
        exact_runtime_plan.moves,
        [](const auto& move) { return move.stable_key == 195U; }));

    snapshot::detail::HouseGeometryCatalog anchor_chain_geometry;
    for (const auto* room_id :
         {"SourceA", "SourceB", "SourceC", "SourceHang", "Attic"}) {
        anchor_chain_geometry.rooms.push_back({
            .definition_id = std::string("Anchor_") + room_id,
            .room_id = room_id,
            .width = 4,
            .height = 5});
    }
    snapshot::detail::FurnitureInfoCatalog anchor_chain_info;
    anchor_chain_info.records.push_back(AnchoredInfo("tower", 1));
    anchor_chain_info.records.push_back(HangingInfo("hanging"));
    std::vector<std::uint8_t> hanging_base(6U * 7U, 0U);
    hanging_base[6U * 6U + 2U] = 2U;
    auto hanging_live = hanging_base;
    hanging_live[1U * 6U + 3U] = 2U;
    hanging_live[3U * 6U + 3U] = 2U;
    std::vector<std::uint8_t> anchor_target_base(6U * 7U, 0U);
    anchor_target_base[2U] = 2U;
    const std::vector<furniture_planning::FurnitureRoomGrid>
        anchor_chain_grids{
            {"SourceA", 6, 7},
            {"SourceB", 6, 7},
            {"SourceC", 6, 7},
            {"SourceHang", 6, 7, hanging_base, hanging_live},
            {"Attic", 6, 7, anchor_target_base, anchor_target_base}};
    const std::vector<snapshot::detail::FurniturePlacement>
        anchor_chain_furniture{
            Placement(196, "tower", "SourceA", -8, -9),
            Placement(197, "tower", "SourceB", -8, -9),
            Placement(198, "tower", "SourceC", -8, -9),
            Placement(199, "hanging", "SourceHang", -8, -8)};
    const auto anchor_chain_plan = solver.Plan(
        anchor_chain_furniture,
        anchor_chain_geometry,
        anchor_chain_info,
        anchor_chain_grids);
    AC_CHECK(anchor_chain_plan.target_room_id == "Attic");
    AC_CHECK(anchor_chain_plan.deferred_furniture_count == 0);
    AC_CHECK(anchor_chain_plan.unsupported_furniture_count == 0);
    const auto anchor_chain_final = FinalPlacementStates(
        anchor_chain_furniture, anchor_chain_plan);
    AC_CHECK(std::ranges::all_of(
        anchor_chain_final,
        [](const auto& entry) {
            return entry.second.room == "Attic";
        }));
    AC_CHECK(std::ranges::any_of(
        anchor_chain_plan.moves,
        [](const auto& move) {
            return move.stable_key == 199U &&
                move.target_room_id == "Attic";
        }));

    snapshot::detail::HouseGeometryCatalog batch_geometry;
    batch_geometry.rooms.push_back({
        .definition_id = "BA",
        .room_id = "SourceA",
        .width = 2,
        .height = 2});
    batch_geometry.rooms.push_back({
        .definition_id = "BB",
        .room_id = "SourceB",
        .width = 2,
        .height = 2});
    batch_geometry.rooms.push_back({
        .definition_id = "BT",
        .room_id = "Attic",
        .width = 4,
        .height = 2});
    const std::vector<furniture_planning::FurnitureRoomGrid> batch_grids{
        {"SourceA", 4, 4},
        {"SourceB", 4, 4},
        {"Attic", 6, 4}};
    const std::vector<snapshot::detail::FurniturePlacement> batch_furniture{
        Placement(201, "large", "SourceA", -10, -9),
        Placement(202, "large", "SourceA", -8, -9),
        Placement(203, "large", "SourceB", -10, -9),
        Placement(204, "large", "SourceB", -8, -9),
        Placement(205, "large", "SourceA", -10, -9),
        Placement(206, "large", "SourceA", -8, -9),
        Placement(207, "large", "SourceB", -10, -9),
        Placement(208, "large", "SourceB", -8, -9),
        Placement(209, "large", "SourceA", -10, -9),
        Placement(210, "large", "SourceB", -8, -9)};
    const auto attic_batch = solver.Plan(
        batch_furniture, batch_geometry, info, batch_grids);
    AC_CHECK(attic_batch.target_room_id == "Attic");
    AC_CHECK(attic_batch.planned_room_count == 1);
    AC_CHECK(attic_batch.deferred_furniture_count > 0);
    const auto attic_final = FinalPlacementStates(
        batch_furniture, attic_batch);
    const auto attic_count = std::ranges::count_if(
        attic_final,
        [](const auto& entry) {
            return entry.second.room == "Attic";
        });
    AC_CHECK(attic_count > 0);
    AC_CHECK(attic_count + attic_batch.deferred_furniture_count ==
        batch_furniture.size());

    auto batch_with_unsupported = batch_furniture;
    batch_with_unsupported.push_back(
        Placement(211, "unknown", "SourceA", -7, -9));
    const auto attic_with_unsupported = solver.Plan(
        batch_with_unsupported, batch_geometry, info, batch_grids);
    AC_CHECK(attic_with_unsupported.unsupported_furniture_count == 1);
    AC_CHECK(attic_with_unsupported.deferred_furniture_count >=
        attic_batch.deferred_furniture_count);

    auto rearranged_batch = batch_furniture;
    std::swap(rearranged_batch[0].position_x,
              rearranged_batch[1].position_x);
    std::swap(rearranged_batch[2].position_x,
              rearranged_batch[3].position_x);
    const auto rearranged_attic = solver.Plan(
        rearranged_batch, batch_geometry, info, batch_grids);
    AC_CHECK(rearranged_attic.target_room_id == "Attic");
    const auto rearranged_final = FinalPlacementStates(
        rearranged_batch, rearranged_attic);
    for (const auto& [key, state] : attic_final) {
        if (state.room == "Attic") {
            AC_CHECK(rearranged_final.at(key) == state);
        }
    }

    auto after_attic = batch_furniture;
    for (auto& placement : after_attic) {
        const auto state = attic_final.at(
            static_cast<std::uint64_t>(placement.instance_id));
        placement.room_id = state.room;
        placement.position_x = state.position.first;
        placement.position_y = state.position.second;
    }
    const auto next_batch = solver.Plan(
        after_attic,
        batch_geometry,
        info,
        batch_grids,
        {"Attic"});
    AC_CHECK(next_batch.target_room_id != "Attic");
    AC_CHECK(std::ranges::none_of(
        next_batch.moves,
        [&attic_final](const auto& move) {
            return attic_final.at(move.stable_key).room == "Attic";
        }));

    snapshot::detail::HouseGeometryCatalog locked_attic_geometry;
    locked_attic_geometry.rooms = {
        {.definition_id = "LockedAttic",
         .room_id = "Attic", .width = 2, .height = 1},
        {.definition_id = "OrdinaryRoom",
         .room_id = "Floor1_Small", .width = 4, .height = 2}};
    const std::vector<snapshot::detail::FurniturePlacement>
        locked_attic_furniture{
            Placement(701, "large", "Floor1_Small", -6, -9),
            Placement(702, "small", "Floor1_Small", -6, -9)};
    const std::vector<furniture_planning::FurnitureRoomGrid>
        locked_attic_grids{
            {"Attic", 2, 1},
            {"Floor1_Small", 6, 4}};
    const std::vector<snapshot::detail::FurniturePlacement>
        attic_can_take_other_room_furniture{
            Placement(690, "small", "Floor1_Small", -6, -9)};
    snapshot::detail::HouseGeometryCatalog cross_room_geometry;
    cross_room_geometry.rooms = {
        {.definition_id = "CrossAttic",
         .room_id = "Attic", .width = 4, .height = 2},
        {.definition_id = "CrossOrdinary",
         .room_id = "Floor1_Small", .width = 4, .height = 2}};
    const std::vector<furniture_planning::FurnitureRoomGrid>
        cross_room_grids{
            {"Attic", 6, 4},
            {"Floor1_Small", 6, 4}};
    const auto attic_first_batch = solver.Plan(
        attic_can_take_other_room_furniture,
        cross_room_geometry,
        info,
        cross_room_grids);
    AC_CHECK(attic_first_batch.target_room_id == "Attic");
    AC_CHECK(std::ranges::any_of(
        attic_first_batch.moves,
        [](const auto& move) {
            return move.stable_key == 690U &&
                move.from_room_id == "Floor1_Small" &&
                move.target_room_id == "Attic";
        }));

    const std::vector<snapshot::detail::FurniturePlacement>
        locked_attic_candidate_freeze{
            Placement(691, "small", "Attic", -10, -12),
            Placement(692, "large", "", 0, 0)};
    const auto after_attic_batch = solver.Plan(
        locked_attic_candidate_freeze,
        locked_attic_geometry,
        info,
        locked_attic_grids,
        {"Attic"});
    AC_CHECK(after_attic_batch.target_room_id == "Floor1_Small");
    AC_CHECK(std::ranges::none_of(
        after_attic_batch.moves,
        [](const auto& move) {
            return move.stable_key == 691U;
        }));
    const auto ordinary_room_batch = solver.Plan(
        locked_attic_furniture,
        locked_attic_geometry,
        info,
        locked_attic_grids,
        {"Attic"});
    AC_CHECK(ordinary_room_batch.target_room_id == "Floor1_Small");
    AC_CHECK(ordinary_room_batch.planned_room_count == 1U);
    AC_CHECK(!ordinary_room_batch.moves.empty());
    AC_CHECK(ordinary_room_batch.evacuation_blocked_room_count == 0U);
    AC_CHECK(std::ranges::all_of(
        ordinary_room_batch.moves,
        [](const auto& move) {
            return move.from_room_id == "Floor1_Small" &&
                move.target_room_id == "Floor1_Small";
        }));

    const std::vector<snapshot::detail::FurniturePlacement>
        warehouse_room_fill{
            Placement(801, "large", "Floor1_Small", -6, -9),
            Placement(802, "small", "", 0, 0)};
    const auto warehouse_room_batch = solver.Plan(
        warehouse_room_fill,
        locked_attic_geometry,
        info,
        locked_attic_grids,
        {"Attic"});
    AC_CHECK(warehouse_room_batch.target_room_id == "Floor1_Small");
    AC_CHECK(!warehouse_room_batch.moves.empty());
    AC_CHECK(std::ranges::any_of(
        warehouse_room_batch.moves,
        [](const auto& move) {
            return furniture_planning::IsWarehouseLayoutMove(move);
        }));
    {
        std::set<std::uint64_t> moved_keys;
        for (const auto& move : warehouse_room_batch.moves) {
            AC_CHECK(moved_keys.insert(move.stable_key).second);
        }
    }
    AC_CHECK(warehouse_room_batch.exhausted_room_ids.empty());

    auto purpose_info = info;
    purpose_info.records.push_back(SmallInfo("neutral"));
    purpose_info.records.push_back(SmallInfo("fight-idol"));
    const std::vector<snapshot::detail::FurniturePlacement>
        purpose_warehouse_fill{
            Placement(811, "large", "Floor1_Small", -6, -9),
            Placement(812, "neutral", "", 0, 0),
            Placement(813, "fight-idol", "", 0, 0)};
    const snapshot::detail::FurnitureCatalog purpose_effects{
        {"large", snapshot::RoomAttributes{
            .comfort = 7, .health = 1}},
        {"neutral", snapshot::RoomAttributes{}},
        {"fight-idol", snapshot::RoomAttributes{
            .comfort = -5, .health = 0}}};
    const std::vector<room_planning::RoomPurposeAssignment>
        fight_purpose{{
            .room_id = "Floor1_Small",
            .role = room_planning::RoomRole::CombatStaging,
            .expected_resident_count = 4}};
    const auto purpose_warehouse_batch = solver.Plan(
        purpose_warehouse_fill,
        locked_attic_geometry,
        purpose_info,
        locked_attic_grids,
        {"Attic"},
        purpose_effects,
        fight_purpose);
    AC_CHECK(std::ranges::any_of(
        purpose_warehouse_batch.moves,
        [](const auto& move) {
            return move.stable_key == 813U &&
                furniture_planning::IsWarehouseLayoutMove(move);
        }));

    const std::vector<room_planning::RoomPurposeAssignment>
        purpose_priority{{
            .room_id = "Attic",
            .role = room_planning::RoomRole::General,
            .expected_resident_count = 1},
        {
            .room_id = "Floor1_Small",
            .role = room_planning::RoomRole::Breeding,
            .expected_resident_count = 2}};
    const auto purpose_priority_batch = solver.Plan(
        purpose_warehouse_fill,
        locked_attic_geometry,
        purpose_info,
        locked_attic_grids,
        {},
        purpose_effects,
        purpose_priority);
    AC_CHECK(purpose_priority_batch.target_room_id == "Floor1_Small");

    const std::vector<snapshot::detail::FurniturePlacement> focus_furniture{
        Placement(814, "fight-idol", "", 0, 0)};
    const std::vector<room_planning::RoomPurposeAssignment> focus_purposes{
        {
            .room_id = "Attic",
            .role = room_planning::RoomRole::Breeding,
            .expected_resident_count = 2},
        {
            .room_id = "Floor1_Small",
            .role = room_planning::RoomRole::CombatStaging,
            .expected_resident_count = 4}};
    const auto focused_room_batch = solver.Plan(
        focus_furniture,
        locked_attic_geometry,
        purpose_info,
        locked_attic_grids,
        {},
        purpose_effects,
        focus_purposes,
        {},
        {},
        "Floor1_Small");
    AC_CHECK(focused_room_batch.target_room_id == "Floor1_Small");
    AC_CHECK(focused_room_batch.moves.size() == 1U);

    const auto tabu_baseline = solver.Plan(
        purpose_warehouse_fill,
        locked_attic_geometry,
        purpose_info,
        locked_attic_grids,
        {"Attic"},
        purpose_effects,
        fight_purpose);
    AC_CHECK(!tabu_baseline.moves.empty());
    if (!tabu_baseline.moves.empty()) {
        const auto tabu_alternative = solver.Plan(
            purpose_warehouse_fill,
            locked_attic_geometry,
            purpose_info,
            locked_attic_grids,
            {"Attic"},
            purpose_effects,
            fight_purpose,
            {},
            {tabu_baseline.moves.front()},
            "Floor1_Small");
        AC_CHECK(tabu_alternative.target_room_id == "Floor1_Small");
        AC_CHECK(!tabu_alternative.moves.empty());
        if (!tabu_alternative.moves.empty()) {
            AC_CHECK(tabu_alternative.moves.front() !=
                tabu_baseline.moves.front());
        }
        AC_CHECK(tabu_alternative.tabu_filtered_move_count > 0U);
        AC_CHECK(std::ranges::find(
            tabu_alternative.exhausted_room_ids,
            "Floor1_Small") == tabu_alternative.exhausted_room_ids.end());
    }

    const std::vector<snapshot::detail::FurniturePlacement>
        neutral_only_fill{
            Placement(821, "large", "Floor1_Small", -6, -9),
            Placement(822, "neutral", "", 0, 0)};
    const auto neutral_only_batch = solver.Plan(
        neutral_only_fill,
        locked_attic_geometry,
        purpose_info,
        locked_attic_grids,
        {"Attic"},
        purpose_effects,
        fight_purpose);
    AC_CHECK(std::ranges::none_of(
        neutral_only_batch.moves,
        [](const auto& move) {
            return move.stable_key == 822U &&
                furniture_planning::IsWarehouseLayoutMove(move);
        }));
    auto fill_remaining_config =
        furniture_planning::FurniturePlacementConfig{};
    fill_remaining_config.fill_remaining_capacity = true;
    const auto neutral_fill_enabled = solver.Plan(
        neutral_only_fill,
        locked_attic_geometry,
        purpose_info,
        locked_attic_grids,
        {"Attic"},
        purpose_effects,
        fight_purpose,
        fill_remaining_config);
    AC_CHECK(std::ranges::any_of(
        neutral_fill_enabled.moves,
        [](const auto& move) {
            return move.stable_key == 822U &&
                furniture_planning::IsWarehouseLayoutMove(move);
        }));

    snapshot::detail::HouseGeometryCatalog efficient_purpose_geometry;
    efficient_purpose_geometry.rooms = {
        {.definition_id = "LockedAtticEfficient",
         .room_id = "Attic", .width = 1, .height = 1},
        {.definition_id = "EfficientPurpose",
         .room_id = "Floor1_Small", .width = 2, .height = 1}};
    auto efficient_purpose_info = purpose_info;
    efficient_purpose_info.records.push_back(
        WidePosterInfo("wide-neutral"));
    efficient_purpose_info.records.push_back(
        PosterInfo("compact-neutral"));
    const std::vector<snapshot::detail::FurniturePlacement>
        efficient_purpose_furniture{
            Placement(823, "wide-neutral", "", 0, 0),
            Placement(824, "compact-neutral", "", 0, 0)};
    const auto efficient_purpose_batch = solver.Plan(
        efficient_purpose_furniture,
        efficient_purpose_geometry,
        efficient_purpose_info,
        {{"Attic", 1, 1, {0U}, {0U}},
         {"Floor1_Small", 2, 1, {0U, 0U}, {0U, 0U}}},
        {"Attic"},
        purpose_effects,
        fight_purpose);
    AC_CHECK(efficient_purpose_batch.target_room_id == "Floor1_Small");
    AC_CHECK(!efficient_purpose_batch.moves.empty());
    AC_CHECK(std::ranges::any_of(
        efficient_purpose_batch.moves,
        [](const auto& move) {
            return move.stable_key == 824U &&
                furniture_planning::IsWarehouseLayoutMove(move);
        }));
    AC_CHECK(std::ranges::none_of(
        efficient_purpose_batch.moves,
        [](const auto& move) {
            return move.stable_key == 823U;
        }));

    const std::vector<snapshot::detail::FurniturePlacement>
        combined_purpose_furniture{
            Placement(825, "wide-neutral", "", 0, 0),
            Placement(826, "compact-neutral", "", 0, 0),
            Placement(827, "compact-neutral", "", 0, 0)};
    const snapshot::detail::FurnitureCatalog combined_purpose_effects{
        {"wide-neutral", snapshot::RoomAttributes{
            .comfort = 8, .stimulation = 8}},
        {"compact-neutral", snapshot::RoomAttributes{
            .comfort = 5, .stimulation = 5}}};
    const std::vector<room_planning::RoomPurposeAssignment>
        combined_breeding_purpose{{
            .room_id = "Floor1_Small",
            .role = room_planning::RoomRole::Breeding,
            .expected_resident_count = 2}};
    const auto combined_purpose_batch = solver.Plan(
        combined_purpose_furniture,
        efficient_purpose_geometry,
        efficient_purpose_info,
        {{"Attic", 1, 1, {0U}, {0U}},
         {"Floor1_Small", 2, 1, {0U, 0U}, {0U, 0U}}},
        {"Attic"},
        combined_purpose_effects,
        combined_breeding_purpose);
    AC_CHECK(std::ranges::none_of(
        combined_purpose_batch.moves,
        [](const auto& move) {
            return move.stable_key == 825U;
        }));
    AC_CHECK(std::ranges::count_if(
        combined_purpose_batch.moves,
        [](const auto& move) {
            return (move.stable_key == 826U ||
                    move.stable_key == 827U) &&
                furniture_planning::IsWarehouseLayoutMove(move);
        }) == 1);

    const std::vector<snapshot::detail::FurniturePlacement>
        satisfied_breeding_fill{
            Placement(831, "large", "Floor1_Small", -6, -9),
            Placement(832, "neutral", "Floor1_Small", -5, -9),
            Placement(833, "small", "", 0, 0)};
    const snapshot::detail::FurnitureCatalog satisfied_breeding_effects{
        {"large", snapshot::RoomAttributes{
            .comfort = 4, .stimulation = 4}},
        {"neutral", snapshot::RoomAttributes{}},
        {"small", snapshot::RoomAttributes{
            .comfort = 1, .stimulation = 1}}};
    const std::vector<room_planning::RoomPurposeAssignment>
        satisfied_breeding_purpose{{
            .room_id = "Floor1_Small",
            .role = room_planning::RoomRole::Breeding,
            .expected_resident_count = 2}};
    const auto satisfied_breeding_batch = solver.Plan(
        satisfied_breeding_fill,
        locked_attic_geometry,
        purpose_info,
        locked_attic_grids,
        {"Attic"},
        satisfied_breeding_effects,
        satisfied_breeding_purpose);
    AC_CHECK(std::ranges::none_of(
        satisfied_breeding_batch.moves,
        [](const auto& move) {
            return furniture_planning::IsWarehouseLayoutMove(move);
        }));

    snapshot::detail::HouseGeometryCatalog sparse_purpose_geometry;
    sparse_purpose_geometry.rooms = {{
        .definition_id = "SparsePurpose",
        .room_id = "SparsePurpose",
        .width = 10,
        .height = 5}};
    const std::vector<furniture_planning::FurnitureRoomGrid>
        sparse_purpose_grids{{"SparsePurpose", 12, 7}};
    const std::vector<snapshot::detail::FurniturePlacement>
        sparse_purpose_fill{
            Placement(834, "large", "SparsePurpose", -6, -9),
            Placement(835, "neutral", "", 0, 0)};
    const std::vector<room_planning::RoomPurposeAssignment>
        sparse_breeding_purpose{{
            .room_id = "SparsePurpose",
            .role = room_planning::RoomRole::Breeding,
            .expected_resident_count = 2}};
    const auto sparse_purpose_batch = solver.Plan(
        sparse_purpose_fill,
        sparse_purpose_geometry,
        purpose_info,
        sparse_purpose_grids,
        {},
        satisfied_breeding_effects,
        sparse_breeding_purpose);
    AC_CHECK(std::ranges::any_of(
        sparse_purpose_batch.moves,
        [](const auto& move) {
            return move.stable_key == 835U &&
                furniture_planning::IsWarehouseLayoutMove(move);
        }));

    const std::vector<snapshot::detail::FurniturePlacement>
        empty_breeding_fill{
            Placement(841, "small", "", 0, 0)};
    const auto empty_breeding_batch = solver.Plan(
        empty_breeding_fill,
        locked_attic_geometry,
        purpose_info,
        locked_attic_grids,
        {"Attic"},
        satisfied_breeding_effects,
        satisfied_breeding_purpose);
    AC_CHECK(empty_breeding_batch.moves.size() == 1U);
    AC_CHECK(furniture_planning::IsWarehouseLayoutMove(
        empty_breeding_batch.moves.front()));

    std::vector<snapshot::detail::FurniturePlacement>
        warehouse_priority_furniture{
            Placement(901, "large", "Floor1_Small", -6, -9),
            Placement(902, "small", "Floor2_Large", -6, -9)};
    for (std::int64_t key = 903; key < 935; ++key) {
        warehouse_priority_furniture.push_back(
            Placement(key, "small", "", 0, 0));
    }
    const auto warehouse_priority_batch = solver.Plan(
        warehouse_priority_furniture,
        locked_attic_geometry,
        info,
        locked_attic_grids,
        {"Attic"});
    AC_CHECK(!warehouse_priority_batch.moves.empty());
    AC_CHECK(std::ranges::any_of(
        warehouse_priority_batch.moves,
        [](const auto& move) {
            return furniture_planning::IsWarehouseLayoutMove(move);
        }));

    const std::vector<snapshot::detail::FurniturePlacement>
        blocked_room_furniture{
            Placement(950, "large", "Floor1_Small", -6, -9)};
    auto blocked_room_grids = locked_attic_grids;
    blocked_room_grids[1].base_cells.assign(24U, 0U);
    blocked_room_grids[1].live_cells.assign(24U, 0U);
    blocked_room_grids[1].live_cells[0] = 3U;
    const auto blocked_room_batch = solver.Plan(
        blocked_room_furniture,
        locked_attic_geometry,
        info,
        blocked_room_grids,
        {"Attic"});
    AC_CHECK(blocked_room_batch.moves.empty());
    AC_CHECK(blocked_room_batch.exhausted_room_ids.empty());
    AC_CHECK(blocked_room_batch.current_state_blocked_room_count != 0U ||
        blocked_room_batch.installation_blocked_room_count != 0U ||
        blocked_room_batch.unsupported_furniture_count != 0U);

    const auto repeated = solver.Plan(first_layout, geometry, info);
    AC_CHECK(repeated.moves.size() == first.moves.size());
    for (std::size_t index = 0; index < first.moves.size(); ++index) {
        AC_CHECK(repeated.moves[index].stable_key ==
            first.moves[index].stable_key);
        AC_CHECK(repeated.moves[index].from_x == first.moves[index].from_x);
        AC_CHECK(repeated.moves[index].from_y == first.moves[index].from_y);
        AC_CHECK(repeated.moves[index].target_x ==
            first.moves[index].target_x);
        AC_CHECK(repeated.moves[index].target_y ==
            first.moves[index].target_y);
    }

    auto unsupported = first_layout;
    unsupported[0].scale_x = 0;
    const auto blocked = solver.Plan(unsupported, geometry, info);
    AC_CHECK(blocked.moves.empty());
    AC_CHECK(blocked.unsupported_furniture_count == 4);
    AC_CHECK(blocked.warehouse_furniture_count == 1);

    const furniture_planning::FurniturePlacementConfig placement_config;
    const auto combat_exact = furniture_planning::RankFurniturePurpose(
        room_planning::RoomRole::CombatStaging,
        snapshot::RoomAttributes{
            .comfort = -8, .stimulation = 0, .health = 0, .mutation = 8},
        4,
        placement_config);
    const auto combat_overdone = furniture_planning::RankFurniturePurpose(
        room_planning::RoomRole::CombatStaging,
        snapshot::RoomAttributes{
            .comfort = -15, .stimulation = 38, .health = 0, .mutation = 0},
        4,
        placement_config);
    AC_CHECK(combat_exact > combat_overdone);

    const auto combat_exact_one_resident =
        furniture_planning::RankFurniturePurpose(
            room_planning::RoomRole::CombatStaging,
            snapshot::RoomAttributes{
                .comfort = -8,
                .stimulation = 0,
                .health = 0,
                .mutation = 8},
            1,
            placement_config);
    const auto combat_exact_six_residents =
        furniture_planning::RankFurniturePurpose(
            room_planning::RoomRole::CombatStaging,
            snapshot::RoomAttributes{
                .comfort = -8,
                .stimulation = 0,
                .health = 0,
                .mutation = 8},
            6,
            placement_config);
    AC_CHECK(combat_exact_one_resident == combat_exact_six_residents);

    auto configured_whole_room_target = placement_config;
    configured_whole_room_target.combat.mutation_per_resident = 12.0;
    const room_planning::RoomPurposeAssignment one_resident_combat{
        .room_id = "CombatOne",
        .role = room_planning::RoomRole::CombatStaging,
        .expected_resident_count = 1};
    const room_planning::RoomPurposeAssignment six_resident_combat{
        .room_id = "CombatSix",
        .role = room_planning::RoomRole::CombatStaging,
        .expected_resident_count = 6};
    const snapshot::RoomAttributes configured_combat_target{
        .comfort = -8, .stimulation = 0, .health = 0, .mutation = 12};
    AC_CHECK(!furniture_planning::FurniturePurposeNeedsMore(
        &one_resident_combat,
        configured_combat_target,
        configured_whole_room_target));
    AC_CHECK(!furniture_planning::FurniturePurposeNeedsMore(
        &six_resident_combat,
        configured_combat_target,
        configured_whole_room_target));
    AC_CHECK(
        furniture_planning::RankFurniturePurpose(
            &one_resident_combat,
            configured_combat_target,
            configured_whole_room_target) ==
        furniture_planning::RankFurniturePurpose(
            &six_resident_combat,
            configured_combat_target,
            configured_whole_room_target));

    const auto combat_mutation = furniture_planning::RankFurniturePurpose(
        room_planning::RoomRole::CombatStaging,
        snapshot::RoomAttributes{
            .comfort = -8, .stimulation = 0, .health = 0, .mutation = 8},
        4,
        placement_config);
    const auto combat_stimulation = furniture_planning::RankFurniturePurpose(
        room_planning::RoomRole::CombatStaging,
        snapshot::RoomAttributes{
            .comfort = -8, .stimulation = 8, .health = 0, .mutation = 0},
        4,
        placement_config);
    AC_CHECK(combat_mutation > combat_stimulation);

    const auto recovery_target = furniture_planning::RankFurniturePurpose(
        room_planning::RoomRole::Recovery,
        snapshot::RoomAttributes{
            .comfort = 8, .stimulation = 0, .health = 8, .mutation = 0},
        4,
        placement_config);
    const auto recovery_health_excess =
        furniture_planning::RankFurniturePurpose(
            room_planning::RoomRole::Recovery,
            snapshot::RoomAttributes{
                .comfort = 8, .stimulation = 0, .health = 14, .mutation = 0},
            4,
            placement_config);
    AC_CHECK(recovery_target > recovery_health_excess);
}

}  // namespace autocattery::tests
