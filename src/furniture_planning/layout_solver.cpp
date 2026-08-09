#include "auto_cattery/furniture_planning/layout_solver.hpp"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <tuple>
#include <unordered_map>
#include <utility>

namespace autocattery::furniture_planning {
namespace {

using snapshot::detail::FurnitureInfoRecord;
using snapshot::detail::FurniturePlacement;
using snapshot::detail::FurniturePlacementTile;
using snapshot::detail::RoomCollisionGrid;
using snapshot::detail::RoomGeometryDefinition;

constexpr std::int64_t kEmptyOwner = -1;
constexpr std::size_t kPackingBeamWidth = 128;
constexpr std::size_t kCandidateBranchesPerState = 16;

struct OffsetCell {
    std::int32_t x{};
    std::int32_t y{};
    FurniturePlacementTile tile{FurniturePlacementTile::Empty};
};

struct MappedCell {
    std::int32_t x{};
    std::int32_t y{};
    FurniturePlacementTile tile{FurniturePlacementTile::Empty};
};

struct LayoutItem {
    const FurniturePlacement* placement{};
    std::vector<OffsetCell> offsets;
    std::vector<MappedCell> current_cells;
    std::vector<std::vector<MappedCell>> candidates;
    std::size_t solid_count{};
    std::size_t support_count{};
    bool passive{};
};

struct Occupancy {
    std::vector<std::int64_t> solid_owner;
    std::vector<std::int64_t> support_owner;
};

struct PackState {
    Occupancy occupancy;
    std::vector<std::optional<std::size_t>> candidate_by_item;
    std::size_t room_support_count{};
    bool has_bounds{};
    std::int32_t min_x{};
    std::int32_t max_x{};
    std::int32_t min_y{};
    std::int32_t max_y{};
    std::int64_t coordinate_sum{};
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

bool IsRecognized(FurniturePlacementTile tile) {
    return tile == FurniturePlacementTile::Empty ||
        tile == FurniturePlacementTile::Hitbox ||
        tile == FurniturePlacementTile::Solid ||
        tile == FurniturePlacementTile::Support;
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
            const auto tile = info.placement_grid.At(x, y);
            if (tile == FurniturePlacementTile::Empty) {
                continue;
            }
            if (!IsRecognized(tile)) {
                cells.clear();
                return cells;
            }
            cells.push_back({
                static_cast<std::int32_t>(x) * placement.scale_x,
                static_cast<std::int32_t>(y) * placement.scale_y,
                tile});
        }
    }
    return cells;
}

bool MapCells(
    std::int32_t origin_x,
    std::int32_t origin_y,
    const std::vector<OffsetCell>& offsets,
    std::vector<MappedCell>& cells) {
    cells.clear();
    cells.reserve(offsets.size());
    for (const auto& offset : offsets) {
        std::int32_t x{};
        std::int32_t y{};
        if (!CheckedCoordinate(origin_x, offset.x, x) ||
            !CheckedCoordinate(origin_y, offset.y, y)) {
            cells.clear();
            return false;
        }
        cells.push_back({x, y, offset.tile});
    }
    return true;
}

bool MapCells(
    const FurniturePlacement& placement,
    const std::vector<OffsetCell>& offsets,
    std::vector<MappedCell>& cells) {
    return MapCells(
        placement.position_x,
        placement.position_y,
        offsets,
        cells);
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

bool GeometricallyAllowed(
    const RoomCollisionGrid& room,
    const std::vector<MappedCell>& cells) {
    for (const auto& cell : cells) {
        if (!Inside(room, cell.x, cell.y)) {
            return false;
        }
        const auto room_value = room.At(
            static_cast<std::size_t>(cell.x),
            static_cast<std::size_t>(cell.y));
        if (cell.tile == FurniturePlacementTile::Support) {
            if (room_value != 0U && room_value != 2U) {
                return false;
            }
        } else if (room_value != 0U) {
            return false;
        }
    }
    return true;
}

std::vector<std::vector<MappedCell>> GenerateCandidates(
    const RoomCollisionGrid& room,
    const LayoutItem& item) {
    std::vector<std::vector<MappedCell>> candidates;
    const auto [min_x, min_y] = MinimumOffsets(item.offsets);
    const auto [max_x, max_y] = MaximumOffsets(item.offsets);
    const auto first_x = -static_cast<std::int64_t>(min_x);
    const auto first_y = -static_cast<std::int64_t>(min_y);
    const auto last_x = static_cast<std::int64_t>(room.width) - 1 -
        static_cast<std::int64_t>(max_x);
    const auto last_y = static_cast<std::int64_t>(room.height) - 1 -
        static_cast<std::int64_t>(max_y);
    if (first_x > last_x || first_y > last_y ||
        first_x < std::numeric_limits<std::int32_t>::min() ||
        last_x > std::numeric_limits<std::int32_t>::max() ||
        first_y < std::numeric_limits<std::int32_t>::min() ||
        last_y > std::numeric_limits<std::int32_t>::max()) {
        return candidates;
    }
    std::vector<MappedCell> cells;
    for (auto y = first_y; y <= last_y; ++y) {
        for (auto x = first_x; x <= last_x; ++x) {
            if (MapCells(
                    static_cast<std::int32_t>(x),
                    static_cast<std::int32_t>(y),
                    item.offsets,
                    cells) &&
                GeometricallyAllowed(room, cells)) {
                candidates.push_back(cells);
            }
        }
    }
    return candidates;
}

bool CanPlace(
    const RoomCollisionGrid& room,
    const Occupancy& occupancy,
    const std::vector<MappedCell>& cells) {
    for (const auto& cell : cells) {
        const auto index = CellIndex(room, cell.x, cell.y);
        if (cell.tile == FurniturePlacementTile::Solid) {
            if (occupancy.solid_owner[index] != kEmptyOwner) {
                return false;
            }
        } else if (cell.tile == FurniturePlacementTile::Support) {
            if (occupancy.support_owner[index] != kEmptyOwner ||
                (room.At(
                     static_cast<std::size_t>(cell.x),
                     static_cast<std::size_t>(cell.y)) != 2U &&
                 occupancy.solid_owner[index] == kEmptyOwner)) {
                return false;
            }
        }
    }
    return true;
}

void Place(
    const RoomCollisionGrid& room,
    std::int64_t owner,
    const std::vector<MappedCell>& cells,
    Occupancy& occupancy) {
    for (const auto& cell : cells) {
        const auto index = CellIndex(room, cell.x, cell.y);
        if (cell.tile == FurniturePlacementTile::Solid) {
            occupancy.solid_owner[index] = owner;
        } else if (cell.tile == FurniturePlacementTile::Support) {
            occupancy.support_owner[index] = owner;
        }
    }
}

void Remove(
    const RoomCollisionGrid& room,
    std::int64_t owner,
    const std::vector<MappedCell>& cells,
    Occupancy& occupancy) {
    for (const auto& cell : cells) {
        const auto index = CellIndex(room, cell.x, cell.y);
        if (cell.tile == FurniturePlacementTile::Solid &&
            occupancy.solid_owner[index] == owner) {
            occupancy.solid_owner[index] = kEmptyOwner;
        } else if (cell.tile == FurniturePlacementTile::Support &&
                   occupancy.support_owner[index] == owner) {
            occupancy.support_owner[index] = kEmptyOwner;
        }
    }
}

bool BuildOccupancy(
    const RoomCollisionGrid& room,
    const std::vector<LayoutItem>& items,
    const std::vector<std::vector<MappedCell>>& cells_by_item,
    Occupancy& occupancy) {
    occupancy.solid_owner.assign(room.width * room.height, kEmptyOwner);
    occupancy.support_owner.assign(room.width * room.height, kEmptyOwner);
    for (std::size_t index = 0; index < items.size(); ++index) {
        for (const auto& cell : cells_by_item[index]) {
            if (!Inside(room, cell.x, cell.y) ||
                !GeometricallyAllowed(room, cells_by_item[index])) {
                return false;
            }
            if (cell.tile == FurniturePlacementTile::Solid) {
                const auto cell_index = CellIndex(room, cell.x, cell.y);
                if (occupancy.solid_owner[cell_index] != kEmptyOwner) {
                    return false;
                }
                occupancy.solid_owner[cell_index] =
                    items[index].placement->instance_id;
            }
        }
    }
    for (std::size_t index = 0; index < items.size(); ++index) {
        for (const auto& cell : cells_by_item[index]) {
            if (cell.tile != FurniturePlacementTile::Support) {
                continue;
            }
            const auto cell_index = CellIndex(room, cell.x, cell.y);
            if (occupancy.support_owner[cell_index] != kEmptyOwner ||
                (room.At(
                     static_cast<std::size_t>(cell.x),
                     static_cast<std::size_t>(cell.y)) != 2U &&
                 occupancy.solid_owner[cell_index] == kEmptyOwner)) {
                return false;
            }
            occupancy.support_owner[cell_index] =
                items[index].placement->instance_id;
        }
    }
    return true;
}

void ExpandBounds(PackState& state, const std::vector<MappedCell>& cells) {
    for (const auto& cell : cells) {
        if (cell.tile != FurniturePlacementTile::Solid &&
            cell.tile != FurniturePlacementTile::Support) {
            continue;
        }
        if (!state.has_bounds) {
            state.has_bounds = true;
            state.min_x = state.max_x = cell.x;
            state.min_y = state.max_y = cell.y;
        } else {
            state.min_x = std::min(state.min_x, cell.x);
            state.max_x = std::max(state.max_x, cell.x);
            state.min_y = std::min(state.min_y, cell.y);
            state.max_y = std::max(state.max_y, cell.y);
        }
        state.coordinate_sum +=
            static_cast<std::int64_t>(cell.x) + cell.y;
    }
}

std::tuple<std::int64_t, std::int32_t, std::int32_t> BoundsScore(
    const PackState& state) {
    if (!state.has_bounds) {
        return {0, 0, 0};
    }
    const auto width = state.max_x - state.min_x + 1;
    const auto height = state.max_y - state.min_y + 1;
    return {
        static_cast<std::int64_t>(width) * height,
        width,
        height};
}

bool BetterPackState(const PackState& left, const PackState& right) {
    if (left.room_support_count != right.room_support_count) {
        return left.room_support_count < right.room_support_count;
    }
    const auto left_bounds = BoundsScore(left);
    const auto right_bounds = BoundsScore(right);
    if (left_bounds != right_bounds) {
        return left_bounds < right_bounds;
    }
    if (left.coordinate_sum != right.coordinate_sum) {
        return left.coordinate_sum < right.coordinate_sum;
    }
    return left.candidate_by_item < right.candidate_by_item;
}

std::optional<PackState> PackInOrder(
    const RoomCollisionGrid& room,
    const std::vector<LayoutItem>& items,
    const std::vector<std::size_t>& order) {
    PackState initial;
    initial.occupancy.solid_owner.assign(
        room.width * room.height, kEmptyOwner);
    initial.occupancy.support_owner.assign(
        room.width * room.height, kEmptyOwner);
    initial.candidate_by_item.resize(items.size());
    std::vector<PackState> beam{std::move(initial)};
    for (const auto item_index : order) {
        const auto& item = items[item_index];
        std::vector<PackState> expanded;
        for (const auto& state : beam) {
            std::vector<PackState> local;
            for (std::size_t candidate_index = 0;
                 candidate_index < item.candidates.size();
                 ++candidate_index) {
                const auto& candidate = item.candidates[candidate_index];
                if (!CanPlace(room, state.occupancy, candidate)) {
                    continue;
                }
                auto next = state;
                next.candidate_by_item[item_index] = candidate_index;
                for (const auto& cell : candidate) {
                    if (cell.tile == FurniturePlacementTile::Support &&
                        room.At(
                            static_cast<std::size_t>(cell.x),
                            static_cast<std::size_t>(cell.y)) == 2U) {
                        ++next.room_support_count;
                    }
                }
                Place(
                    room,
                    item.placement->instance_id,
                    candidate,
                    next.occupancy);
                ExpandBounds(next, candidate);
                local.push_back(std::move(next));
            }
            std::ranges::sort(local, BetterPackState);
            if (local.size() > kCandidateBranchesPerState) {
                local.resize(kCandidateBranchesPerState);
            }
            expanded.insert(
                expanded.end(),
                std::make_move_iterator(local.begin()),
                std::make_move_iterator(local.end()));
        }
        if (expanded.empty()) {
            return std::nullopt;
        }
        std::ranges::sort(expanded, BetterPackState);
        if (expanded.size() > kPackingBeamWidth) {
            expanded.resize(kPackingBeamWidth);
        }
        beam = std::move(expanded);
    }
    return beam.front();
}

std::vector<std::vector<std::size_t>> PackingOrders(
    const std::vector<LayoutItem>& items) {
    std::vector<std::vector<std::size_t>> orders;
    std::vector<std::size_t> base(items.size());
    for (std::size_t index = 0; index < items.size(); ++index) {
        base[index] = index;
    }
    const auto add = [&](auto comparator) {
        auto order = base;
        std::ranges::sort(order, comparator);
        if (std::ranges::find(orders, order) == orders.end()) {
            orders.push_back(std::move(order));
        }
    };
    add([&items](std::size_t left, std::size_t right) {
        return std::tuple{
                   items[left].solid_count,
                   items[left].support_count,
                   items[left].offsets.size(),
                   -items[left].placement->instance_id} >
            std::tuple{
                   items[right].solid_count,
                   items[right].support_count,
                   items[right].offsets.size(),
                   -items[right].placement->instance_id};
    });
    add([&items](std::size_t left, std::size_t right) {
        return std::tuple{
                   items[left].support_count,
                   items[left].solid_count,
                   items[left].offsets.size(),
                   -items[left].placement->instance_id} >
            std::tuple{
                   items[right].support_count,
                   items[right].solid_count,
                   items[right].offsets.size(),
                   -items[right].placement->instance_id};
    });
    add([&items](std::size_t left, std::size_t right) {
        return std::tuple{
                   items[left].offsets.size(),
                   items[left].solid_count,
                   items[left].support_count,
                   -items[left].placement->instance_id} >
            std::tuple{
                   items[right].offsets.size(),
                   items[right].solid_count,
                   items[right].support_count,
                   -items[right].placement->instance_id};
    });
    add([&items](std::size_t left, std::size_t right) {
        return items[left].placement->instance_id <
            items[right].placement->instance_id;
    });
    return orders;
}

std::pair<std::int32_t, std::int32_t> OriginOf(
    const LayoutItem& item,
    const std::vector<MappedCell>& cells) {
    return {
        cells.front().x - item.offsets.front().x,
        cells.front().y - item.offsets.front().y};
}

std::vector<std::set<std::size_t>> Dependencies(
    const RoomCollisionGrid& room,
    const std::vector<LayoutItem>& items,
    const std::vector<std::vector<MappedCell>>& cells_by_item,
    const Occupancy& occupancy) {
    std::unordered_map<std::int64_t, std::size_t> index_by_owner;
    for (std::size_t index = 0; index < items.size(); ++index) {
        index_by_owner.emplace(items[index].placement->instance_id, index);
    }
    std::vector<std::set<std::size_t>> dependencies(items.size());
    for (std::size_t index = 0; index < items.size(); ++index) {
        for (const auto& cell : cells_by_item[index]) {
            if (cell.tile != FurniturePlacementTile::Support ||
                room.At(
                    static_cast<std::size_t>(cell.x),
                    static_cast<std::size_t>(cell.y)) == 2U) {
                continue;
            }
            const auto owner = occupancy.solid_owner[
                CellIndex(room, cell.x, cell.y)];
            const auto found = index_by_owner.find(owner);
            if (found != index_by_owner.end() && found->second != index) {
                dependencies[index].insert(found->second);
            }
        }
    }
    return dependencies;
}

bool HasCurrentDependents(
    std::size_t provider,
    const std::vector<std::set<std::size_t>>& dependencies) {
    return std::ranges::any_of(
        dependencies,
        [provider](const auto& item_dependencies) {
            return item_dependencies.contains(provider);
        });
}

bool CandidateAvoidsFinalTargets(
    const RoomCollisionGrid& room,
    std::size_t item_index,
    const std::vector<MappedCell>& candidate,
    const std::vector<std::vector<MappedCell>>& final_cells) {
    std::set<std::size_t> occupied;
    for (const auto& cell : candidate) {
        if (cell.tile == FurniturePlacementTile::Solid ||
            cell.tile == FurniturePlacementTile::Support) {
            occupied.insert(CellIndex(room, cell.x, cell.y));
        }
    }
    for (std::size_t index = 0; index < final_cells.size(); ++index) {
        if (index == item_index) {
            continue;
        }
        for (const auto& cell : final_cells[index]) {
            if ((cell.tile == FurniturePlacementTile::Solid ||
                 cell.tile == FurniturePlacementTile::Support) &&
                occupied.contains(CellIndex(room, cell.x, cell.y))) {
                return false;
            }
        }
    }
    return true;
}

bool AppendExecutionMoves(
    const RoomCollisionGrid& room,
    const snapshot::RoomId& room_id,
    const std::vector<LayoutItem>& items,
    const std::vector<std::vector<MappedCell>>& final_cells,
    std::vector<FurnitureLayoutMove>& moves,
    std::size_t& kept_count) {
    std::vector<std::vector<MappedCell>> current_cells;
    current_cells.reserve(items.size());
    for (const auto& item : items) {
        current_cells.push_back(item.current_cells);
    }
    Occupancy current_occupancy;
    Occupancy final_occupancy;
    if (!BuildOccupancy(room, items, current_cells, current_occupancy) ||
        !BuildOccupancy(room, items, final_cells, final_occupancy)) {
        return false;
    }
    const auto final_dependencies = Dependencies(
        room, items, final_cells, final_occupancy);
    const auto initial_dependencies = Dependencies(
        room, items, current_cells, current_occupancy);
    std::vector<std::pair<std::int32_t, std::int32_t>> current_origins;
    std::vector<std::pair<std::int32_t, std::int32_t>> final_origins;
    std::vector<bool> pending(items.size());
    current_origins.reserve(items.size());
    final_origins.reserve(items.size());
    for (std::size_t index = 0; index < items.size(); ++index) {
        current_origins.push_back({
            items[index].placement->position_x,
            items[index].placement->position_y});
        final_origins.push_back(OriginOf(items[index], final_cells[index]));
        pending[index] = current_origins.back() != final_origins.back();
    }
    bool dependency_pending_changed = true;
    while (dependency_pending_changed) {
        dependency_pending_changed = false;
        for (std::size_t index = 0; index < items.size(); ++index) {
            const bool provider_moves = std::ranges::any_of(
                initial_dependencies[index],
                [&pending](std::size_t provider) {
                    return pending[provider];
                });
            if (!pending[index] &&
                (initial_dependencies[index] != final_dependencies[index] ||
                 provider_moves)) {
                pending[index] = true;
                dependency_pending_changed = true;
            }
        }
    }
    const auto originally_pending = pending;
    const auto first_move = moves.size();
    std::vector<bool> evacuated(items.size());
    for (std::size_t index = 0; index < items.size(); ++index) {
        evacuated[index] = !pending[index];
    }

    while (std::ranges::any_of(
        evacuated, [](bool value) { return !value; })) {
        const auto current_dependencies = Dependencies(
            room, items, current_cells, current_occupancy);
        bool progressed{};
        for (std::size_t index = 0; index < items.size(); ++index) {
            if (evacuated[index] ||
                HasCurrentDependents(index, current_dependencies)) {
                continue;
            }
            const auto room_anchored = [&room](
                const std::vector<MappedCell>& cells) {
                return std::ranges::all_of(
                    cells,
                    [&room](const auto& cell) {
                        return cell.tile !=
                                FurniturePlacementTile::Support ||
                            room.At(
                                static_cast<std::size_t>(cell.x),
                                static_cast<std::size_t>(cell.y)) == 2U;
                    });
            };
            const bool current_room_anchored =
                room_anchored(current_cells[index]);
            if (current_room_anchored &&
                CandidateAvoidsFinalTargets(
                    room, index, current_cells[index], final_cells)) {
                evacuated[index] = true;
                progressed = true;
                break;
            }

            auto trial_without = current_occupancy;
            Remove(
                room,
                items[index].placement->instance_id,
                current_cells[index],
                trial_without);
            std::vector<std::size_t> staging_candidates;
            for (std::size_t candidate_index = 0;
                 candidate_index < items[index].candidates.size();
                 ++candidate_index) {
                const auto& candidate =
                    items[index].candidates[candidate_index];
                const auto origin = OriginOf(items[index], candidate);
                if (origin == current_origins[index] ||
                    origin == final_origins[index] ||
                    !room_anchored(candidate) ||
                    !CanPlace(room, trial_without, candidate)) {
                    continue;
                }
                staging_candidates.push_back(candidate_index);
            }
            std::ranges::sort(
                staging_candidates,
                [&room, &items, &final_cells, index](
                    std::size_t left,
                    std::size_t right) {
                    const auto score = [&](std::size_t candidate_index) {
                        const auto& candidate =
                            items[index].candidates[candidate_index];
                        std::int64_t coordinate_sum{};
                        for (const auto& cell : candidate) {
                            coordinate_sum +=
                                static_cast<std::int64_t>(cell.x) + cell.y;
                        }
                        return std::tuple{
                            CandidateAvoidsFinalTargets(
                                room, index, candidate, final_cells)
                                ? 0
                                : 1,
                            coordinate_sum,
                            candidate_index};
                    };
                    return score(left) < score(right);
                });
            const bool has_safe_staging = std::ranges::any_of(
                staging_candidates,
                [&room, &items, &final_cells, index](
                    std::size_t candidate_index) {
                    return CandidateAvoidsFinalTargets(
                        room,
                        index,
                        items[index].candidates[candidate_index],
                        final_cells);
                });
            if (current_room_anchored && !has_safe_staging) {
                evacuated[index] = true;
                progressed = true;
                break;
            }
            if (staging_candidates.empty()) {
                continue;
            }
            const auto candidate_index = staging_candidates.front();
            const auto& candidate = items[index].candidates[candidate_index];
            const auto from = current_origins[index];
            const auto target = OriginOf(items[index], candidate);
            moves.push_back({
                static_cast<std::uint64_t>(
                    items[index].placement->instance_id),
                items[index].placement->item_id,
                room_id,
                from.first,
                from.second,
                target.first,
                target.second});
            current_occupancy = std::move(trial_without);
            Place(
                room,
                items[index].placement->instance_id,
                candidate,
                current_occupancy);
            current_cells[index] = candidate;
            current_origins[index] = target;
            evacuated[index] = true;
            progressed = true;
            break;
        }
        if (!progressed) {
            moves.resize(first_move);
            return false;
        }
    }

    while (std::ranges::any_of(pending, [](bool value) { return value; })) {
        bool progressed{};
        for (std::size_t index = 0; index < items.size(); ++index) {
            if (!pending[index]) {
                continue;
            }
            const bool providers_ready = std::ranges::all_of(
                final_dependencies[index],
                [&pending](std::size_t provider) {
                    return !pending[provider];
                });
            if (!providers_ready) {
                continue;
            }
            auto trial = current_occupancy;
            Remove(
                room,
                items[index].placement->instance_id,
                current_cells[index],
                trial);
            if (!CanPlace(room, trial, final_cells[index])) {
                continue;
            }
            const auto from = current_origins[index];
            const auto target = final_origins[index];
            if (from != target) {
                moves.push_back({
                    static_cast<std::uint64_t>(
                        items[index].placement->instance_id),
                    items[index].placement->item_id,
                    room_id,
                    from.first,
                    from.second,
                    target.first,
                    target.second});
            }
            current_occupancy = std::move(trial);
            Place(
                room,
                items[index].placement->instance_id,
                final_cells[index],
                current_occupancy);
            current_cells[index] = final_cells[index];
            current_origins[index] = target;
            pending[index] = false;
            progressed = true;
            break;
        }
        if (!progressed) {
            moves.resize(first_move);
            return false;
        }
    }
    for (std::size_t index = 0; index < items.size(); ++index) {
        if (!originally_pending[index]) {
            ++kept_count;
        }
    }
    return true;
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

        std::vector<LayoutItem> movable;
        movable.reserve(placements.size());
        bool room_supported = true;
        std::size_t passive_count{};
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
                !GeometricallyAllowed(room, item.current_cells)) {
                room_supported = false;
                break;
            }
            item.solid_count = static_cast<std::size_t>(
                std::ranges::count_if(item.offsets, [](const auto& cell) {
                    return cell.tile == FurniturePlacementTile::Solid;
                }));
            item.support_count = static_cast<std::size_t>(
                std::ranges::count_if(item.offsets, [](const auto& cell) {
                    return cell.tile == FurniturePlacementTile::Support;
                }));
            item.passive = item.solid_count == 0U &&
                item.support_count == 0U;
            if (item.passive) {
                ++passive_count;
                continue;
            }
            if (item.support_count == 0U) {
                room_supported = false;
                break;
            }
            item.candidates = GenerateCandidates(room, item);
            if (item.candidates.empty()) {
                room_supported = false;
                break;
            }
            movable.push_back(std::move(item));
        }
        if (!room_supported) {
            plan.unsupported_furniture_count += placements.size();
            continue;
        }

        plan.considered_furniture_count += movable.size() + passive_count;
        plan.kept_furniture_count += passive_count;
        if (movable.empty()) {
            continue;
        }

        std::optional<PackState> best;
        for (const auto& order : PackingOrders(movable)) {
            const auto packed = PackInOrder(room, movable, order);
            if (packed && (!best || BetterPackState(*packed, *best))) {
                best = *packed;
            }
        }
        if (!best) {
            plan.no_space_furniture_count += movable.size();
            continue;
        }

        std::vector<std::vector<MappedCell>> final_cells(movable.size());
        bool complete = true;
        for (std::size_t index = 0; index < movable.size(); ++index) {
            if (!best->candidate_by_item[index]) {
                complete = false;
                break;
            }
            final_cells[index] = movable[index].candidates[
                *best->candidate_by_item[index]];
        }
        if (!complete) {
            plan.no_space_furniture_count += movable.size();
            continue;
        }

        const auto first_move = plan.moves.size();
        auto kept = plan.kept_furniture_count;
        if (!AppendExecutionMoves(
                room,
                room_id,
                movable,
                final_cells,
                plan.moves,
                kept)) {
            plan.moves.resize(first_move);
            plan.unsupported_furniture_count += movable.size();
            continue;
        }
        plan.kept_furniture_count = kept;
        ++plan.planned_room_count;
    }
    return plan;
}

}  // namespace autocattery::furniture_planning
