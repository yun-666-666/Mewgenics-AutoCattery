#include "auto_cattery/furniture_planning/layout_solver.hpp"

#include "test_support.hpp"

#include <algorithm>
#include <cstddef>
#include <map>
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
        if (item.instance_id > 0 && !item.room_id.empty()) {
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
        if (item.instance_id > 0 && !item.room_id.empty()) {
            states[static_cast<std::uint64_t>(item.instance_id)] = {
                item.room_id, {item.position_x, item.position_y}};
        }
    }
    for (const auto& move : plan.moves) {
        const auto current = states.find(move.stable_key);
        AC_CHECK(current != states.end());
        if (current == states.end()) {
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
        AC_CHECK(exact_mobile_move->target_x == -5);
        AC_CHECK(exact_mobile_move->target_y == -9);
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
    AC_CHECK(attic_with_unsupported.deferred_furniture_count ==
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
}

}  // namespace autocattery::tests
