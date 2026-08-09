#include "furniture_placement_gateway.hpp"
#include "mew_ui_furniture_move_adapter.h"

#include "test_support.hpp"

namespace autocattery::tests {

void RunMewUiFurniturePlacementTests() {
    AC_CHECK(AcMewFurnitureWorldAxis(100.0, -6, 1.0) == 106.0);
    AC_CHECK(AcMewFurnitureWorldAxis(100.0, 3, 1.0) == 115.0);
    AC_CHECK(AcMewFurnitureWorldAxis(100.0, -6, -1.0) == 83.0);

    double world_x{};
    double world_y{};
    double world_z{9.0};
    AcMewFurnitureWorldPosition(
        100.0, 200.0, 3, -9, 1.0, 1.0,
        &world_x, &world_y, &world_z);
    AC_CHECK(world_x == 115.0);
    AC_CHECK(world_y == 203.0);
    AC_CHECK(world_z == 0.0);

    const auto missing = AcMewFindFurniturePiece(
        nullptr, "object_cattree1", 5U, 1);
    AC_CHECK(missing.status == AC_MEW_FURNITURE_FIND_INVALID);

    ui::FurniturePlacementGateway gateway;
    const auto location = gateway.Locate({"object_cattree1", 5U});
    AC_CHECK(location.status ==
        ui::FurniturePlacementLookupStatus::Unsupported);
    const auto move = gateway.MoveSameRoom({
        {"object_cattree1", 5U}, 3, -9});
    AC_CHECK(move.status ==
        ui::FurniturePlacementMoveStatus::Unsupported);
}

}  // namespace autocattery::tests
