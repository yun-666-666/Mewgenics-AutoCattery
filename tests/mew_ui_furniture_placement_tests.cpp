#include "furniture_placement_gateway.hpp"
#include "mew_ui_furniture_move_adapter.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <span>
#include <string_view>

#include "test_support.hpp"

namespace autocattery::tests {

void RunMewUiFurniturePlacementTests() {
    alignas(void*) std::array<std::byte, 0x20> component{};
    alignas(void*) std::array<std::byte, 0x20> component_context{};
    void* component_context_pointer = component_context.data();
    std::memcpy(
        component.data() + 0x18,
        &component_context_pointer,
        sizeof(component_context_pointer));
    component_context[0x18] = std::byte{0};
    component_context[0x19] = std::byte{0x7F};
    AC_CHECK(AcMewComponentDeleteQueued(component.data()) == 0);
    component_context[0x18] = std::byte{1};
    AC_CHECK(AcMewComponentDeleteQueued(component.data()) == 1);

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

    AcMewFurnitureCoordinate candidates[64]{};
    const auto candidate_count = AcMewFurnitureCandidatePath(
        4, -8, -4, -8, candidates, 64);
    AC_CHECK(candidate_count > 0);
    AC_CHECK(candidates[0].x == -4);
    AC_CHECK(candidates[0].y == -8);
    AC_CHECK(std::ranges::any_of(
        std::span{candidates, candidate_count},
        [](const auto& candidate) {
            return candidate.x == 3 && candidate.y == -9;
        }));
    AC_CHECK(std::ranges::none_of(
        std::span{candidates, candidate_count},
        [](const auto& candidate) {
            return candidate.x == 4 && candidate.y == -8;
        }));

    AcMewFurnitureCoordinate nearby[16]{};
    const auto nearby_count = AcMewFurnitureNearbyCandidates(
        4, -8, nearby, 16);
    AC_CHECK(nearby_count == 16);
    AC_CHECK(nearby[0].x == 4);
    AC_CHECK(nearby[0].y == -8);
    AC_CHECK(std::ranges::any_of(
        std::span{nearby, nearby_count},
        [](const auto& candidate) {
            return candidate.x == 5 && candidate.y == -8;
        }));

    const auto missing = AcMewFindFurniturePiece(
        nullptr, "object_cattree1", 5U, 1);
    AC_CHECK(missing.status == AC_MEW_FURNITURE_FIND_INVALID);

    ui::FurniturePlacementGateway gateway;
    const auto location = gateway.Locate({"object_cattree1", 5U});
    AC_CHECK(location.status ==
        ui::FurniturePlacementLookupStatus::Unsupported);
    const auto move = gateway.MoveSameRoom({
        .locator = {"object_cattree1", 5U},
        .target_x = 3,
        .target_y = -9});
    AC_CHECK(move.status ==
        ui::FurniturePlacementMoveStatus::Unsupported);
    const auto replacement = gateway.ReplaceWithWarehouse({
        .placed = {"weak-chair", 1U},
        .warehouse_item = "strong-chair",
        .warehouse_stable_key = 2U});
    AC_CHECK(replacement.status ==
        ui::FurnitureWarehouseReplacementStatus::Unsupported);
    AC_CHECK(std::string_view{
        ui::FurnitureWarehouseReplacementStatusName(
            ui::FurnitureWarehouseReplacementStatus::Replaced)} ==
        "replaced");
}

}  // namespace autocattery::tests
