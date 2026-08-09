#include "auto_cattery/furniture_planning/layout_solver.hpp"

#include "test_support.hpp"

#include <cstddef>
#include <string>
#include <utility>

namespace autocattery::tests {
namespace {

snapshot::detail::FurnitureInfoRecord Info(
    std::string item,
    std::size_t width,
    std::size_t height) {
    snapshot::detail::FurnitureInfoRecord info;
    info.item_id = std::move(item);
    info.placement_grid.supported = true;
    for (std::size_t y = 0; y < height; ++y) {
        for (std::size_t x = 0; x < width; ++x) {
            info.placement_grid.tiles[
                (10U + y) * snapshot::detail::kFurniturePlacementGridWidth +
                (10U + x)] =
                snapshot::detail::FurniturePlacementTile::Solid;
        }
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

}  // namespace

void RunFurnitureLayoutSolverTests() {
    snapshot::detail::HouseGeometryCatalog geometry;
    geometry.rooms.push_back({
        .definition_id = "R1",
        .room_id = "RoomA",
        .width = 6,
        .height = 4});
    snapshot::detail::FurnitureInfoCatalog info;
    info.records.push_back(Info("large", 2, 2));
    info.records.push_back(Info("small", 1, 1));

    const std::vector<snapshot::detail::FurniturePlacement> furniture{
        Placement(20, "small", "RoomA", -4, -9),
        Placement(10, "large", "RoomA", -6, -7),
        Placement(30, "small", "", 0, 0)};
    const furniture_planning::FurnitureLayoutSolver solver;
    const auto first = solver.Plan(furniture, geometry, info);
    const auto second = solver.Plan(furniture, geometry, info);
    AC_CHECK(first.planned_room_count == 1);
    AC_CHECK(first.considered_furniture_count == 2);
    AC_CHECK(first.warehouse_furniture_count == 1);
    AC_CHECK(first.unsupported_furniture_count == 0);
    AC_CHECK(first.no_space_furniture_count == 0);
    AC_CHECK(first.moves.size() == 2);
    if (first.moves.size() == 2) {
        AC_CHECK(first.moves[0].stable_key == 10);
        AC_CHECK(first.moves[0].target_x == -9);
        AC_CHECK(first.moves[0].target_y == -9);
        AC_CHECK(first.moves[1].stable_key == 20);
        AC_CHECK(first.moves[1].target_x == -7);
        AC_CHECK(first.moves[1].target_y == -9);
    }
    AC_CHECK(second.moves.size() == first.moves.size());
    for (std::size_t index = 0; index < first.moves.size(); ++index) {
        AC_CHECK(second.moves[index].stable_key == first.moves[index].stable_key);
        AC_CHECK(second.moves[index].target_x == first.moves[index].target_x);
        AC_CHECK(second.moves[index].target_y == first.moves[index].target_y);
    }

    auto supported_info = Info("supported", 1, 1);
    supported_info.placement_grid.tiles[
        9U * snapshot::detail::kFurniturePlacementGridWidth + 10U] =
        snapshot::detail::FurniturePlacementTile::Support;
    snapshot::detail::FurnitureInfoCatalog supported_catalog;
    supported_catalog.records.push_back(std::move(supported_info));
    const auto supported = solver.Plan(
        {Placement(40, "supported", "RoomA", -6, -9)},
        geometry,
        supported_catalog);
    AC_CHECK(supported.moves.size() == 1);
    if (supported.moves.size() == 1) {
        AC_CHECK(supported.moves[0].target_x == -9);
        AC_CHECK(supported.moves[0].target_y == -9);
    }

    auto base_info = Info("base", 1, 1);
    base_info.placement_grid.tiles[
        9U * snapshot::detail::kFurniturePlacementGridWidth + 10U] =
        snapshot::detail::FurniturePlacementTile::Surface;
    snapshot::detail::FurnitureInfoCatalog stacked_catalog;
    stacked_catalog.records.push_back(std::move(base_info));
    stacked_catalog.records.push_back(Info("upper", 1, 1));
    const auto stacked = solver.Plan(
        {Placement(50, "base", "RoomA", -6, -9),
         Placement(60, "upper", "RoomA", -6, -10)},
        geometry,
        stacked_catalog);
    AC_CHECK(stacked.moves.empty());
    AC_CHECK(stacked.considered_furniture_count == 0);
    AC_CHECK(stacked.unsupported_furniture_count == 2);

    auto unsupported = furniture;
    unsupported[0].scale_x = 0;
    const auto blocked = solver.Plan(unsupported, geometry, info);
    AC_CHECK(blocked.moves.empty());
    AC_CHECK(blocked.unsupported_furniture_count == 2);
    AC_CHECK(blocked.warehouse_furniture_count == 1);
}

}  // namespace autocattery::tests
