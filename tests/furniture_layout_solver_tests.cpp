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

snapshot::detail::FurnitureInfoRecord PosterInfo(std::string item) {
    snapshot::detail::FurnitureInfoRecord info;
    info.item_id = std::move(item);
    info.placement_grid.supported = true;
    info.placement_grid.tiles[
        12U * snapshot::detail::kFurniturePlacementGridWidth + 10U] =
        FurniturePlacementTile::Hitbox;
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
    AC_CHECK(
        (packed.at(10).second == -7 || packed.at(30).second == -7));
    AC_CHECK(packed.at(40) == Position(-5, -8));

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
            return move.room_id ==
                one_room_at_a_time.moves.front().room_id;
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
