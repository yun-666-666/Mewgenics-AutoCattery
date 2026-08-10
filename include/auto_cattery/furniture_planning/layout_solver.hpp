#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "auto_cattery/snapshot/detail/furniture_attributes.hpp"
#include "auto_cattery/snapshot/detail/furniture_geometry.hpp"

namespace autocattery::furniture_planning {

struct FurnitureLayoutMove {
    std::uint64_t stable_key{};
    std::string item_id;
    snapshot::RoomId room_id;
    std::int32_t from_x{};
    std::int32_t from_y{};
    std::int32_t target_x{};
    std::int32_t target_y{};
};

struct FurnitureLayoutPlan {
    std::vector<FurnitureLayoutMove> moves;
    std::size_t planned_room_count{};
    std::size_t considered_furniture_count{};
    std::size_t kept_furniture_count{};
    std::size_t warehouse_furniture_count{};
    std::size_t unsupported_furniture_count{};
    std::size_t no_space_furniture_count{};
    std::size_t current_state_blocked_room_count{};
    std::size_t evacuation_blocked_room_count{};
    std::size_t installation_blocked_room_count{};
};

class FurnitureLayoutSolver final {
public:
    [[nodiscard]] FurnitureLayoutPlan Plan(
        const std::vector<snapshot::detail::FurniturePlacement>& furniture,
        const snapshot::detail::HouseGeometryCatalog& geometry,
        const snapshot::detail::FurnitureInfoCatalog& furniture_info) const;
};

}  // namespace autocattery::furniture_planning
