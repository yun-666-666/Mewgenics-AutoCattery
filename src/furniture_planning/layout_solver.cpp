#include "auto_cattery/furniture_planning/layout_solver.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <unordered_map>
#include <utility>

namespace autocattery::furniture_planning {
namespace {

using snapshot::detail::FurnitureInfoRecord;
using snapshot::detail::FurniturePlacement;
using snapshot::detail::FurniturePlacementTile;
using snapshot::detail::RoomCollisionGrid;
using snapshot::detail::RoomGeometryDefinition;

struct OffsetCell {
    std::int32_t x{};
    std::int32_t y{};
    FurniturePlacementTile tile{FurniturePlacementTile::Empty};
    std::uint8_t required_room_value{};
};

struct LayoutItem {
    const FurniturePlacement* placement{};
    std::vector<OffsetCell> offsets;
    std::vector<OffsetCell> current_cells;
    bool floor_supported{};
};

bool CheckedCoordinate(
    std::int32_t origin,
    std::int32_t offset,
    std::int32_t& output) {
    const auto value = static_cast<std::int64_t>(origin) + offset;
    if (value < std::numeric_limits<std::int32_t>::min() ||
        value > std::numeric_limits<std::int32_t>::max()) {
        return false;
    }
    output = static_cast<std::int32_t>(value);
    return true;
}

bool Inside(
    const RoomCollisionGrid& room,
    std::int32_t x,
    std::int32_t y) {
    return x >= 0 && y >= 0 &&
        static_cast<std::size_t>(x) < room.width &&
        static_cast<std::size_t>(y) < room.height;
}

std::size_t CellIndex(
    const RoomCollisionGrid& room,
    std::int32_t x,
    std::int32_t y) {
    return static_cast<std::size_t>(y) * room.width +
        static_cast<std::size_t>(x);
}

bool ConsumesOccupancy(FurniturePlacementTile tile) {
    return tile == FurniturePlacementTile::Hitbox ||
        tile == FurniturePlacementTile::Solid;
}

std::vector<OffsetCell> ActiveOffsets(
    const FurniturePlacement& placement,
    const FurnitureInfoRecord& info) {
    std::vector<OffsetCell> cells;
    if (!placement.HasSupportedGridScale() ||
        !info.placement_grid.supported) {
        return cells;
    }
    for (std::size_t y = 0;
         y < snapshot::detail::kFurniturePlacementGridHeight;
         ++y) {
        for (std::size_t x = 0;
             x < snapshot::detail::kFurniturePlacementGridWidth;
             ++x) {
            if (info.placement_grid.At(x, y) ==
                FurniturePlacementTile::Empty) {
                continue;
            }
            cells.push_back({
                static_cast<std::int32_t>(x) * placement.scale_x,
                static_cast<std::int32_t>(y) * placement.scale_y,
                info.placement_grid.At(x, y)});
        }
    }
    return cells;
}

bool MapCells(
    const FurniturePlacement& placement,
    const std::vector<OffsetCell>& offsets,
    std::vector<OffsetCell>& cells) {
    cells.clear();
    cells.reserve(offsets.size());
    for (const auto& offset : offsets) {
        std::int32_t x{};
        std::int32_t y{};
        if (!CheckedCoordinate(placement.position_x, offset.x, x) ||
            !CheckedCoordinate(placement.position_y, offset.y, y)) {
            cells.clear();
            return false;
        }
        cells.push_back({x, y, offset.tile, offset.required_room_value});
    }
    return true;
}

bool AddOccupancy(
    const RoomCollisionGrid& room,
    const std::vector<OffsetCell>& cells,
    std::vector<std::size_t>& occupancy) {
    for (const auto& cell : cells) {
        if (!Inside(room, cell.x, cell.y)) {
            return false;
        }
    }
    for (const auto& cell : cells) {
        if (ConsumesOccupancy(cell.tile)) {
            ++occupancy[CellIndex(room, cell.x, cell.y)];
        }
    }
    return true;
}

void RemoveOccupancy(
    const RoomCollisionGrid& room,
    const std::vector<OffsetCell>& cells,
    std::vector<std::size_t>& occupancy) {
    for (const auto& cell : cells) {
        if (!ConsumesOccupancy(cell.tile)) {
            continue;
        }
        auto& count = occupancy[CellIndex(room, cell.x, cell.y)];
        if (count != 0U) {
            --count;
        }
    }
}

bool FloorSupported(
    const RoomCollisionGrid& room,
    std::vector<OffsetCell>& offsets,
    const std::vector<OffsetCell>& cells) {
    bool has_solid{};
    for (std::size_t index = 0; index < cells.size(); ++index) {
        const auto& cell = cells[index];
        if (!Inside(room, cell.x, cell.y)) {
            return false;
        }
        const auto room_value = room.At(
            static_cast<std::size_t>(cell.x),
            static_cast<std::size_t>(cell.y));
        if (room_value != 0U && room_value != 2U) {
            return false;
        }
        offsets[index].required_room_value = room_value;
        has_solid = has_solid ||
            offsets[index].tile == FurniturePlacementTile::Solid;
    }
    return has_solid;
}

bool CandidateAvailable(
    const RoomCollisionGrid& room,
    const std::vector<OffsetCell>& offsets,
    const std::vector<std::size_t>& occupancy,
    std::int32_t target_x,
    std::int32_t target_y,
    std::vector<OffsetCell>& cells) {
    cells.clear();
    cells.reserve(offsets.size());
    for (const auto& offset : offsets) {
        std::int32_t x{};
        std::int32_t y{};
        if (!CheckedCoordinate(target_x, offset.x, x) ||
            !CheckedCoordinate(target_y, offset.y, y) ||
            !Inside(room, x, y) ||
            room.At(
                static_cast<std::size_t>(x),
                static_cast<std::size_t>(y)) !=
                offset.required_room_value ||
            (ConsumesOccupancy(offset.tile) &&
             occupancy[CellIndex(room, x, y)] != 0U)) {
            cells.clear();
            return false;
        }
        cells.push_back({
            x, y, offset.tile, offset.required_room_value});
    }
    return true;
}

std::pair<std::int32_t, std::int32_t> MinimumOffsets(
    const std::vector<OffsetCell>& offsets) {
    auto min_x = offsets.front().x;
    auto min_y = offsets.front().y;
    for (const auto& cell : offsets) {
        min_x = std::min(min_x, cell.x);
        min_y = std::min(min_y, cell.y);
    }
    return {min_x, min_y};
}

std::pair<std::int32_t, std::int32_t> MaximumOffsets(
    const std::vector<OffsetCell>& offsets) {
    auto max_x = offsets.front().x;
    auto max_y = offsets.front().y;
    for (const auto& cell : offsets) {
        max_x = std::max(max_x, cell.x);
        max_y = std::max(max_y, cell.y);
    }
    return {max_x, max_y};
}

}  // namespace

FurnitureLayoutPlan FurnitureLayoutSolver::Plan(
    const std::vector<FurniturePlacement>& furniture,
    const snapshot::detail::HouseGeometryCatalog& geometry,
    const snapshot::detail::FurnitureInfoCatalog& furniture_info) const {
    FurnitureLayoutPlan plan;
    std::unordered_map<std::string, const FurnitureInfoRecord*> info_by_item;
    info_by_item.reserve(furniture_info.records.size());
    for (const auto& info : furniture_info.records) {
        info_by_item.emplace(info.item_id, &info);
    }
    std::unordered_map<snapshot::RoomId, const RoomGeometryDefinition*>
        geometry_by_room;
    geometry_by_room.reserve(geometry.rooms.size());
    for (const auto& room : geometry.rooms) {
        const auto [entry, inserted] =
            geometry_by_room.emplace(room.room_id, &room);
        if (!inserted) {
            entry->second = nullptr;
        }
    }

    std::map<snapshot::RoomId, std::vector<const FurniturePlacement*>>
        by_room;
    for (const auto& placement : furniture) {
        if (placement.room_id.empty()) {
            ++plan.warehouse_furniture_count;
            continue;
        }
        by_room[placement.room_id].push_back(&placement);
    }

    for (const auto& [room_id, placements] : by_room) {
        const auto geometry_entry = geometry_by_room.find(room_id);
        if (geometry_entry == geometry_by_room.end() ||
            geometry_entry->second == nullptr) {
            plan.unsupported_furniture_count += placements.size();
            continue;
        }
        const auto room = snapshot::detail::DecodeRoomCollisionGrid(
            *geometry_entry->second);
        if (!room.supported || room.cells.empty()) {
            plan.unsupported_furniture_count += placements.size();
            continue;
        }

        std::vector<LayoutItem> items;
        items.reserve(placements.size());
        bool room_supported = true;
        for (const auto* placement : placements) {
            const auto info = info_by_item.find(placement->item_id);
            if (placement->instance_id <= 0 ||
                info == info_by_item.end()) {
                room_supported = false;
                break;
            }
            LayoutItem item;
            item.placement = placement;
            item.offsets = ActiveOffsets(*placement, *info->second);
            if (item.offsets.empty() ||
                !MapCells(*placement, item.offsets, item.current_cells) ||
                !std::ranges::all_of(
                    item.current_cells,
                    [&room](const auto& cell) {
                        return Inside(room, cell.x, cell.y);
                    })) {
                room_supported = false;
                break;
            }
            item.floor_supported = FloorSupported(
                room, item.offsets, item.current_cells);
            items.push_back(std::move(item));
        }
        if (!room_supported) {
            plan.unsupported_furniture_count += placements.size();
            continue;
        }

        std::vector<std::size_t> occupancy(room.width * room.height);
        for (const auto& item : items) {
            if (!AddOccupancy(room, item.current_cells, occupancy)) {
                room_supported = false;
                break;
            }
        }
        if (!room_supported) {
            plan.unsupported_furniture_count += placements.size();
            continue;
        }

        std::vector<LayoutItem*> movable;
        movable.reserve(items.size());
        for (auto& item : items) {
            const bool shared_origin = std::ranges::count_if(
                items,
                [&item](const auto& candidate) {
                    return candidate.placement->position_x ==
                               item.placement->position_x &&
                        candidate.placement->position_y ==
                               item.placement->position_y;
                }) > 1;
            const bool isolated = std::ranges::all_of(
                item.current_cells,
                [&room, &occupancy](const auto& cell) {
                    return !ConsumesOccupancy(cell.tile) ||
                        occupancy[CellIndex(room, cell.x, cell.y)] == 1U;
                });
            if (item.floor_supported && isolated && !shared_origin) {
                movable.push_back(&item);
            } else {
                ++plan.unsupported_furniture_count;
            }
        }
        std::ranges::sort(
            movable,
            [](const LayoutItem* left, const LayoutItem* right) {
                if (left->offsets.size() != right->offsets.size()) {
                    return left->offsets.size() > right->offsets.size();
                }
                return left->placement->instance_id <
                    right->placement->instance_id;
            });
        if (!movable.empty()) {
            ++plan.planned_room_count;
        }

        for (auto* item : movable) {
            ++plan.considered_furniture_count;
            RemoveOccupancy(room, item->current_cells, occupancy);
            const auto [min_x, min_y] = MinimumOffsets(item->offsets);
            const auto [max_x, max_y] = MaximumOffsets(item->offsets);
            const auto first_x = -static_cast<std::int64_t>(min_x);
            const auto first_y = -static_cast<std::int64_t>(min_y);
            const auto last_x = static_cast<std::int64_t>(room.width) - 1 -
                static_cast<std::int64_t>(max_x);
            const auto last_y = static_cast<std::int64_t>(room.height) - 1 -
                static_cast<std::int64_t>(max_y);
            bool found{};
            std::int32_t target_x{};
            std::int32_t target_y{};
            std::vector<OffsetCell> target_cells;
            if (first_x <= last_x && first_y <= last_y &&
                first_x >= std::numeric_limits<std::int32_t>::min() &&
                last_x <= std::numeric_limits<std::int32_t>::max() &&
                first_y >= std::numeric_limits<std::int32_t>::min() &&
                last_y <= std::numeric_limits<std::int32_t>::max()) {
                for (auto y = first_y; y <= last_y && !found; ++y) {
                    for (auto x = first_x; x <= last_x; ++x) {
                        if (CandidateAvailable(
                                room,
                                item->offsets,
                                occupancy,
                                static_cast<std::int32_t>(x),
                                static_cast<std::int32_t>(y),
                                target_cells)) {
                            target_x = static_cast<std::int32_t>(x);
                            target_y = static_cast<std::int32_t>(y);
                            found = true;
                            break;
                        }
                    }
                }
            }
            if (!found) {
                AddOccupancy(room, item->current_cells, occupancy);
                ++plan.no_space_furniture_count;
                continue;
            }
            AddOccupancy(room, target_cells, occupancy);
            if (target_x == item->placement->position_x &&
                target_y == item->placement->position_y) {
                ++plan.kept_furniture_count;
                continue;
            }
            plan.moves.push_back({
                static_cast<std::uint64_t>(item->placement->instance_id),
                item->placement->item_id,
                room_id,
                item->placement->position_x,
                item->placement->position_y,
                target_x,
                target_y});
        }
    }
    return plan;
}

}  // namespace autocattery::furniture_planning
