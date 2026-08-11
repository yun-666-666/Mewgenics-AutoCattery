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

constexpr std::size_t kPackingBeamWidth = 16;
constexpr std::size_t kCandidateBranchesPerState = 4;
constexpr std::size_t kWholeHousePackingBeamWidth = 128;
constexpr std::size_t kWholeHouseCandidateBranchesPerState = 12;
constexpr std::size_t kAnchorChainSeedLimit = 32;
constexpr std::size_t kAnchorChainVisitLimit = 100000;

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
    std::size_t hitbox_count{};
    std::size_t solid_count{};
    std::size_t surface_count{};
    std::size_t support_count{};
    std::size_t poop_count{};
    bool passive{};
    bool fixed{};
};

enum class ExecutionPlanResult {
    Success,
    CurrentStateInvalid,
    FinalStateInvalid,
    EvacuationBlocked,
    InstallationBlocked
};

struct Occupancy {
    // Current-build validate/commit semantics (RVAs 0x2EDE60/0x2EE160):
    // Hitbox and Solid write blocking grid values, Support only requires an
    // existing Solid value, Surface is metadata, and PoopLogic writes its own
    // blocking value. Support and Surface therefore never occupy a cell.
    std::vector<std::uint16_t> hitbox_count;
    std::vector<std::uint16_t> solid_count;
    std::vector<std::uint16_t> poop_count;
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
    std::size_t selected_count{};
    std::size_t selected_cell_count{};
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

bool RoomProvidesFloorSupport(
    const RoomCollisionGrid& room,
    std::int32_t x,
    std::int32_t y) {
    return y == 0 &&
        room.At(
            static_cast<std::size_t>(x),
            static_cast<std::size_t>(y)) == 2U;
}

bool RoomProvidesBoundarySupport(
    const RoomCollisionGrid& room,
    std::int32_t x,
    std::int32_t y) {
    return room.At(
        static_cast<std::size_t>(x),
        static_cast<std::size_t>(y)) == 2U;
}

bool RoomBlocksAllFurniture(std::uint8_t value) {
    return value >= 6U && value <= 8U;
}

bool BodyOccupied(const Occupancy& occupancy, std::size_t index) {
    return occupancy.hitbox_count[index] != 0U ||
        occupancy.solid_count[index] != 0U ||
        occupancy.poop_count[index] != 0U;
}

bool SupportSatisfied(
    const RoomCollisionGrid& room,
    const Occupancy& occupancy,
    std::int32_t x,
    std::int32_t y) {
    return RoomProvidesBoundarySupport(room, x, y) ||
        occupancy.solid_count[CellIndex(room, x, y)] != 0U;
}

bool CanKeepExistingPlacement(
    const RoomCollisionGrid& room,
    const Occupancy& occupancy,
    const std::vector<MappedCell>& cells) {
    for (const auto& cell : cells) {
        const auto index = CellIndex(room, cell.x, cell.y);
        if (cell.tile == FurniturePlacementTile::Hitbox ||
            cell.tile == FurniturePlacementTile::Solid) {
            if (BodyOccupied(occupancy, index)) {
                return false;
            }
        } else if (cell.tile == FurniturePlacementTile::PoopLogic) {
            if (occupancy.poop_count[index] != 0U ||
                occupancy.solid_count[index] != 0U ||
                RoomProvidesBoundarySupport(room, cell.x, cell.y)) {
                return false;
            }
        } else if (cell.tile == FurniturePlacementTile::Support) {
            if (!SupportSatisfied(
                    room, occupancy, cell.x, cell.y)) {
                return false;
            }
        }
    }
    return true;
}

bool IsRecognized(FurniturePlacementTile tile) {
    return tile == FurniturePlacementTile::Empty ||
        tile == FurniturePlacementTile::Hitbox ||
        tile == FurniturePlacementTile::Solid ||
        tile == FurniturePlacementTile::Support ||
        tile == FurniturePlacementTile::Surface ||
        tile == FurniturePlacementTile::PoopLogic;
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

std::vector<OffsetCell> RecognizedOffsets(
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
            if (tile == FurniturePlacementTile::Empty ||
                !IsRecognized(tile)) {
                continue;
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
        if (RoomBlocksAllFurniture(room_value)) {
            return false;
        }
        if (cell.tile == FurniturePlacementTile::Support) {
            if (room_value != 0U && room_value != 2U) {
                return false;
            }
        } else if ((cell.tile == FurniturePlacementTile::Hitbox ||
                    cell.tile == FurniturePlacementTile::Solid) &&
                   (room_value == 1U || room_value == 2U ||
                    room_value == 5U)) {
            return false;
        } else if (cell.tile == FurniturePlacementTile::PoopLogic &&
                   (room_value == 2U || room_value == 5U)) {
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
        if (cell.tile == FurniturePlacementTile::Hitbox ||
            cell.tile == FurniturePlacementTile::Solid) {
            if (BodyOccupied(occupancy, index)) {
                return false;
            }
        } else if (cell.tile == FurniturePlacementTile::PoopLogic) {
            if (occupancy.poop_count[index] != 0U ||
                occupancy.solid_count[index] != 0U ||
                RoomProvidesBoundarySupport(room, cell.x, cell.y)) {
                return false;
            }
        } else if (cell.tile == FurniturePlacementTile::Support) {
            if (!SupportSatisfied(
                    room, occupancy, cell.x, cell.y)) {
                return false;
            }
        }
    }
    return true;
}

bool CanPlaceBody(
    const RoomCollisionGrid& room,
    const Occupancy& occupancy,
    const std::vector<MappedCell>& cells) {
    for (const auto& cell : cells) {
        const auto index = CellIndex(room, cell.x, cell.y);
        if (cell.tile == FurniturePlacementTile::Hitbox ||
            cell.tile == FurniturePlacementTile::Solid) {
            if (BodyOccupied(occupancy, index)) {
                return false;
            }
        } else if (cell.tile == FurniturePlacementTile::PoopLogic) {
            if (occupancy.poop_count[index] != 0U ||
                occupancy.solid_count[index] != 0U ||
                RoomProvidesBoundarySupport(room, cell.x, cell.y)) {
                return false;
            }
        }
    }
    return true;
}

void Place(
    const RoomCollisionGrid& room,
    std::int64_t,
    const std::vector<MappedCell>& cells,
    Occupancy& occupancy) {
    for (const auto& cell : cells) {
        const auto index = CellIndex(room, cell.x, cell.y);
        if (cell.tile == FurniturePlacementTile::Hitbox) {
            ++occupancy.hitbox_count[index];
        } else if (cell.tile == FurniturePlacementTile::Solid) {
            ++occupancy.solid_count[index];
        } else if (cell.tile == FurniturePlacementTile::PoopLogic) {
            ++occupancy.poop_count[index];
        }
    }
}

void Remove(
    const RoomCollisionGrid& room,
    std::int64_t,
    const std::vector<MappedCell>& cells,
    Occupancy& occupancy) {
    for (const auto& cell : cells) {
        const auto index = CellIndex(room, cell.x, cell.y);
        if (cell.tile == FurniturePlacementTile::Hitbox &&
            occupancy.hitbox_count[index] != 0U) {
            --occupancy.hitbox_count[index];
        } else if (cell.tile == FurniturePlacementTile::Solid &&
            occupancy.solid_count[index] != 0U) {
            --occupancy.solid_count[index];
        } else if (cell.tile == FurniturePlacementTile::PoopLogic &&
                   occupancy.poop_count[index] != 0U) {
            --occupancy.poop_count[index];
        }
    }
}

bool BuildOccupancy(
    const RoomCollisionGrid& room,
    const std::vector<LayoutItem>& items,
    const std::vector<std::vector<MappedCell>>& cells_by_item,
    Occupancy& occupancy,
    bool allow_existing_conflicts) {
    occupancy.hitbox_count.assign(room.width * room.height, 0U);
    occupancy.solid_count.assign(room.width * room.height, 0U);
    occupancy.poop_count.assign(room.width * room.height, 0U);
    for (std::size_t index = 0; index < items.size(); ++index) {
        for (const auto& cell : cells_by_item[index]) {
            if (!Inside(room, cell.x, cell.y) ||
                (!allow_existing_conflicts && !items[index].fixed &&
                 !GeometricallyAllowed(room, cells_by_item[index]))) {
                return false;
            }
            const auto cell_index = CellIndex(room, cell.x, cell.y);
            if (cell.tile == FurniturePlacementTile::Hitbox ||
                cell.tile == FurniturePlacementTile::Solid) {
                if (!allow_existing_conflicts && !items[index].fixed &&
                    BodyOccupied(occupancy, cell_index)) {
                    return false;
                }
                if (cell.tile == FurniturePlacementTile::Hitbox) {
                    ++occupancy.hitbox_count[cell_index];
                } else {
                    ++occupancy.solid_count[cell_index];
                }
            } else if (cell.tile == FurniturePlacementTile::PoopLogic) {
                if (!allow_existing_conflicts && !items[index].fixed &&
                    (occupancy.poop_count[cell_index] != 0U ||
                     occupancy.solid_count[cell_index] != 0U ||
                     RoomProvidesBoundarySupport(
                         room, cell.x, cell.y))) {
                    return false;
                }
                ++occupancy.poop_count[cell_index];
            }
        }
    }
    for (std::size_t index = 0; index < items.size(); ++index) {
        for (const auto& cell : cells_by_item[index]) {
            if (cell.tile != FurniturePlacementTile::Support) {
                continue;
            }
            if (!allow_existing_conflicts && !items[index].fixed &&
                !SupportSatisfied(
                    room, occupancy, cell.x, cell.y)) {
                return false;
            }
        }
    }
    return true;
}

void ExpandBounds(PackState& state, const std::vector<MappedCell>& cells) {
    for (const auto& cell : cells) {
        if (cell.tile == FurniturePlacementTile::Empty ||
            cell.tile == FurniturePlacementTile::PoopLogic) {
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

std::tuple<
    std::int64_t,
    std::int32_t,
    std::int32_t,
    std::int32_t,
    std::int32_t> BoundsScore(
    const PackState& state) {
    if (!state.has_bounds) {
        return {0, 0, 0, 0, 0};
    }
    const auto width = state.max_x - state.min_x + 1;
    const auto height = state.max_y - state.min_y + 1;
    return {
        static_cast<std::int64_t>(width) * height,
        std::max(width, height),
        width + height,
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

bool BetterFilledPackState(const PackState& left, const PackState& right) {
    if (left.selected_count != right.selected_count) {
        return left.selected_count > right.selected_count;
    }
    if (left.selected_cell_count != right.selected_cell_count) {
        return left.selected_cell_count > right.selected_cell_count;
    }
    return BetterPackState(left, right);
}

std::optional<std::pair<std::int32_t, std::int32_t>>
FirstUnsatisfiedSelectedSupport(
    const RoomCollisionGrid& room,
    const std::vector<LayoutItem>& items,
    const PackState& state) {
    for (std::size_t index = 0; index < items.size(); ++index) {
        if (!state.candidate_by_item[index]) {
            continue;
        }
        const auto& cells = items[index].candidates[
            *state.candidate_by_item[index]];
        for (const auto& cell : cells) {
            if (cell.tile == FurniturePlacementTile::Support &&
                !SupportSatisfied(
                    room, state.occupancy, cell.x, cell.y)) {
                return std::pair{cell.x, cell.y};
            }
        }
    }
    return std::nullopt;
}

bool SelectedPackingIsExecutable(
    const RoomCollisionGrid& room,
    const std::vector<LayoutItem>& items,
    const Occupancy& fixed_occupancy,
    const PackState& state) {
    auto occupancy = fixed_occupancy;
    std::vector<bool> pending(items.size());
    std::size_t pending_count{};
    for (std::size_t index = 0; index < items.size(); ++index) {
        if (state.candidate_by_item[index]) {
            pending[index] = true;
            ++pending_count;
        }
    }
    while (pending_count != 0U) {
        bool progressed{};
        for (std::size_t index = 0; index < items.size(); ++index) {
            if (!pending[index]) {
                continue;
            }
            const auto& cells = items[index].candidates[
                *state.candidate_by_item[index]];
            if (!CanPlace(room, occupancy, cells)) {
                continue;
            }
            Place(
                room,
                items[index].placement->instance_id,
                cells,
                occupancy);
            pending[index] = false;
            --pending_count;
            progressed = true;
        }
        if (!progressed) {
            return false;
        }
    }
    return true;
}

void CollectAnchorChainSeeds(
    const RoomCollisionGrid& room,
    const std::vector<LayoutItem>& items,
    const Occupancy& fixed_occupancy,
    PackState state,
    std::vector<PackState>& seeds,
    std::size_t& visits) {
    if (seeds.size() >= kAnchorChainSeedLimit ||
        ++visits > kAnchorChainVisitLimit) {
        return;
    }
    const auto unsupported = FirstUnsatisfiedSelectedSupport(
        room, items, state);
    if (!unsupported) {
        if (SelectedPackingIsExecutable(
                room, items, fixed_occupancy, state)) {
            seeds.push_back(std::move(state));
        }
        return;
    }

    for (std::size_t item_index = 0;
         item_index < items.size(); ++item_index) {
        if (state.candidate_by_item[item_index] ||
            items[item_index].solid_count == 0U) {
            continue;
        }
        for (std::size_t candidate_index = 0;
             candidate_index < items[item_index].candidates.size();
             ++candidate_index) {
            const auto& candidate =
                items[item_index].candidates[candidate_index];
            const bool provides_support = std::ranges::any_of(
                candidate,
                [&unsupported](const auto& cell) {
                    return cell.tile == FurniturePlacementTile::Solid &&
                        cell.x == unsupported->first &&
                        cell.y == unsupported->second;
                });
            if (!provides_support ||
                !CanPlaceBody(room, state.occupancy, candidate)) {
                continue;
            }
            auto next = state;
            next.candidate_by_item[item_index] = candidate_index;
            ++next.selected_count;
            next.selected_cell_count += items[item_index].offsets.size();
            for (const auto& cell : candidate) {
                if (cell.tile == FurniturePlacementTile::Support &&
                    RoomProvidesFloorSupport(room, cell.x, cell.y)) {
                    ++next.room_support_count;
                }
            }
            Place(
                room,
                items[item_index].placement->instance_id,
                candidate,
                next.occupancy);
            ExpandBounds(next, candidate);
            CollectAnchorChainSeeds(
                room,
                items,
                fixed_occupancy,
                std::move(next),
                seeds,
                visits);
            if (seeds.size() >= kAnchorChainSeedLimit ||
                visits >= kAnchorChainVisitLimit) {
                return;
            }
        }
    }
}

std::vector<PackState> BuildAnchorChainSeeds(
    const RoomCollisionGrid& room,
    const std::vector<LayoutItem>& items,
    const std::vector<LayoutItem>& fixed_items,
    const std::vector<bool>& required) {
    PackState base;
    base.occupancy.hitbox_count.assign(room.width * room.height, 0U);
    base.occupancy.solid_count.assign(room.width * room.height, 0U);
    base.occupancy.poop_count.assign(room.width * room.height, 0U);
    base.candidate_by_item.resize(items.size());
    for (const auto& item : fixed_items) {
        Place(
            room,
            item.placement->instance_id,
            item.current_cells,
            base.occupancy);
        ExpandBounds(base, item.current_cells);
    }
    const auto fixed_occupancy = base.occupancy;
    std::vector<PackState> seeds;
    std::size_t visits{};
    for (std::size_t item_index = 0;
         item_index < items.size(); ++item_index) {
        if (required[item_index] || items[item_index].support_count == 0U) {
            continue;
        }
        const bool directly_anchored = std::ranges::any_of(
            items[item_index].candidates,
            [&room, &fixed_occupancy](const auto& candidate) {
                return CanPlace(room, fixed_occupancy, candidate);
            });
        if (directly_anchored) {
            continue;
        }
        for (std::size_t candidate_index = 0;
             candidate_index < items[item_index].candidates.size();
             ++candidate_index) {
            const auto& candidate =
                items[item_index].candidates[candidate_index];
            if (!CanPlaceBody(room, base.occupancy, candidate)) {
                continue;
            }
            auto seed = base;
            seed.candidate_by_item[item_index] = candidate_index;
            seed.selected_count = 1U;
            seed.selected_cell_count = items[item_index].offsets.size();
            for (const auto& cell : candidate) {
                if (cell.tile == FurniturePlacementTile::Support &&
                    RoomProvidesFloorSupport(room, cell.x, cell.y)) {
                    ++seed.room_support_count;
                }
            }
            Place(
                room,
                items[item_index].placement->instance_id,
                candidate,
                seed.occupancy);
            ExpandBounds(seed, candidate);
            CollectAnchorChainSeeds(
                room,
                items,
                fixed_occupancy,
                std::move(seed),
                seeds,
                visits);
            if (seeds.size() >= kAnchorChainSeedLimit ||
                visits >= kAnchorChainVisitLimit) {
                break;
            }
        }
    }
    std::ranges::sort(seeds, BetterFilledPackState);
    const auto unique_end = std::ranges::unique(
        seeds, {}, &PackState::candidate_by_item);
    seeds.erase(unique_end.begin(), unique_end.end());
    return seeds;
}

std::vector<PackState> PackInOrder(
    const RoomCollisionGrid& room,
    const std::vector<LayoutItem>& items,
    const std::vector<std::size_t>& order) {
    PackState initial;
    initial.occupancy.hitbox_count.assign(room.width * room.height, 0U);
    initial.occupancy.solid_count.assign(room.width * room.height, 0U);
    initial.occupancy.poop_count.assign(room.width * room.height, 0U);
    initial.candidate_by_item.resize(items.size());
    for (std::size_t index = 0; index < items.size(); ++index) {
        if (!items[index].fixed) {
            continue;
        }
        if (items[index].candidates.size() != 1U) {
            return {};
        }
        initial.candidate_by_item[index] = 0U;
        Place(
            room,
            items[index].placement->instance_id,
            items[index].candidates.front(),
            initial.occupancy);
    }
    std::vector<PackState> beam{std::move(initial)};
    for (const auto item_index : order) {
        const auto& item = items[item_index];
        if (item.fixed) {
            continue;
        }
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
                        RoomProvidesFloorSupport(
                            room, cell.x, cell.y)) {
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
            return {};
        }
        std::ranges::sort(expanded, BetterPackState);
        if (expanded.size() > kPackingBeamWidth) {
            expanded.resize(kPackingBeamWidth);
        }
        beam = std::move(expanded);
    }
    return beam;
}

std::vector<PackState> PackSubsetInOrder(
    const RoomCollisionGrid& room,
    const std::vector<LayoutItem>& items,
    const std::vector<LayoutItem>& fixed_items,
    const std::vector<bool>& required,
    const std::vector<std::size_t>& order,
    const std::vector<PackState>* seed_states = nullptr) {
    if (required.size() != items.size()) {
        return {};
    }
    PackState initial;
    initial.occupancy.hitbox_count.assign(room.width * room.height, 0U);
    initial.occupancy.solid_count.assign(room.width * room.height, 0U);
    initial.occupancy.poop_count.assign(room.width * room.height, 0U);
    initial.candidate_by_item.resize(items.size());
    for (const auto& item : fixed_items) {
        if (!std::ranges::all_of(
                item.current_cells,
                [&room](const auto& cell) {
                    return Inside(room, cell.x, cell.y);
                })) {
            return {};
        }
        Place(
            room,
            item.placement->instance_id,
            item.current_cells,
            initial.occupancy);
        ExpandBounds(initial, item.current_cells);
    }
    std::vector<PackState> beam =
        seed_states != nullptr && !seed_states->empty()
        ? *seed_states
        : std::vector<PackState>{std::move(initial)};
    for (const auto item_index : order) {
        const auto& item = items[item_index];
        std::vector<PackState> expanded;
        for (const auto& state : beam) {
            if (state.candidate_by_item[item_index]) {
                expanded.push_back(state);
                continue;
            }
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
                ++next.selected_count;
                next.selected_cell_count += item.offsets.size();
                for (const auto& cell : candidate) {
                    if (cell.tile == FurniturePlacementTile::Support &&
                        RoomProvidesFloorSupport(
                            room, cell.x, cell.y)) {
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
            if (!required[item_index]) {
                local.push_back(state);
            }
            std::ranges::sort(local, BetterFilledPackState);
            if (local.size() > kWholeHouseCandidateBranchesPerState) {
                local.resize(kWholeHouseCandidateBranchesPerState);
            }
            expanded.insert(
                expanded.end(),
                std::make_move_iterator(local.begin()),
                std::make_move_iterator(local.end()));
        }
        if (expanded.empty()) {
            return {};
        }
        std::ranges::sort(expanded, BetterFilledPackState);
        if (expanded.size() > kWholeHousePackingBeamWidth) {
            expanded.resize(kWholeHousePackingBeamWidth);
        }
        beam = std::move(expanded);
    }
    return beam;
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
                   items[left].surface_count,
                   items[left].support_count,
                   items[left].offsets.size(),
                   -items[left].placement->instance_id} >
            std::tuple{
                   items[right].solid_count,
                   items[right].surface_count,
                   items[right].support_count,
                   items[right].offsets.size(),
                   -items[right].placement->instance_id};
    });
    add([&items](std::size_t left, std::size_t right) {
        return std::tuple{
                   items[left].surface_count,
                   items[left].support_count,
                   items[left].solid_count,
                   items[left].offsets.size(),
                   -items[left].placement->instance_id} >
            std::tuple{
                   items[right].surface_count,
                   items[right].support_count,
                   items[right].solid_count,
                   items[right].offsets.size(),
                   -items[right].placement->instance_id};
    });
    add([&items](std::size_t left, std::size_t right) {
        return std::tuple{
                   items[left].candidates.size(),
                   std::numeric_limits<std::size_t>::max() -
                       items[left].solid_count,
                   std::numeric_limits<std::size_t>::max() -
                       items[left].offsets.size(),
                   items[left].placement->instance_id} <
            std::tuple{
                   items[right].candidates.size(),
                   std::numeric_limits<std::size_t>::max() -
                       items[right].solid_count,
                   std::numeric_limits<std::size_t>::max() -
                       items[right].offsets.size(),
                   items[right].placement->instance_id};
    });
    add([&items](std::size_t left, std::size_t right) {
        return items[left].placement->instance_id <
            items[right].placement->instance_id;
    });
    add([&items](std::size_t left, std::size_t right) {
        return items[left].placement->instance_id >
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

bool SameCells(
    const std::vector<MappedCell>& left,
    const std::vector<MappedCell>& right) {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (left[index].x != right[index].x ||
            left[index].y != right[index].y ||
            left[index].tile != right[index].tile) {
            return false;
        }
    }
    return true;
}

std::optional<PackState> CurrentSelectedPackState(
    const RoomCollisionGrid& room,
    const std::vector<LayoutItem>& items,
    const std::vector<LayoutItem>& fixed_items) {
    PackState state;
    state.occupancy.hitbox_count.assign(room.width * room.height, 0U);
    state.occupancy.solid_count.assign(room.width * room.height, 0U);
    state.occupancy.poop_count.assign(room.width * room.height, 0U);
    state.candidate_by_item.resize(items.size());
    for (const auto& item : fixed_items) {
        if (!std::ranges::all_of(
                item.current_cells,
                [&room](const auto& cell) {
                    return Inside(room, cell.x, cell.y);
                })) {
            return std::nullopt;
        }
        Place(
            room,
            item.placement->instance_id,
            item.current_cells,
            state.occupancy);
        ExpandBounds(state, item.current_cells);
    }
    for (std::size_t index = 0; index < items.size(); ++index) {
        const auto candidate = std::ranges::find_if(
            items[index].candidates,
            [&items, index](const auto& cells) {
                return SameCells(cells, items[index].current_cells);
            });
        if (candidate == items[index].candidates.end() ||
            !CanPlaceBody(room, state.occupancy, *candidate)) {
            return std::nullopt;
        }
        const auto candidate_index = static_cast<std::size_t>(
            std::distance(items[index].candidates.begin(), candidate));
        state.candidate_by_item[index] = candidate_index;
        ++state.selected_count;
        state.selected_cell_count += items[index].offsets.size();
        for (const auto& cell : *candidate) {
            if (cell.tile == FurniturePlacementTile::Support &&
                RoomProvidesFloorSupport(room, cell.x, cell.y)) {
                ++state.room_support_count;
            }
        }
        Place(
            room,
            items[index].placement->instance_id,
            *candidate,
            state.occupancy);
        ExpandBounds(state, *candidate);
    }
    for (std::size_t index = 0; index < items.size(); ++index) {
        const auto& cells = items[index].candidates[
            *state.candidate_by_item[index]];
        if (std::ranges::any_of(
                cells,
                [&room, &state](const auto& cell) {
                    return cell.tile == FurniturePlacementTile::Support &&
                        !SupportSatisfied(
                            room, state.occupancy, cell.x, cell.y);
                })) {
            return std::nullopt;
        }
    }
    return state;
}

std::optional<PackState> CurrentPackState(
    const RoomCollisionGrid& room,
    const std::vector<LayoutItem>& items) {
    std::vector<std::vector<MappedCell>> current_cells;
    current_cells.reserve(items.size());
    for (const auto& item : items) {
        current_cells.push_back(item.current_cells);
    }
    Occupancy occupancy;
    if (!BuildOccupancy(
            room, items, current_cells, occupancy, false)) {
        return std::nullopt;
    }

    PackState state;
    state.occupancy = std::move(occupancy);
    state.candidate_by_item.resize(items.size());
    for (std::size_t index = 0; index < items.size(); ++index) {
        const auto candidate = std::ranges::find_if(
            items[index].candidates,
            [&items, index](const auto& cells) {
                return SameCells(cells, items[index].current_cells);
            });
        if (candidate == items[index].candidates.end()) {
            return std::nullopt;
        }
        state.candidate_by_item[index] = static_cast<std::size_t>(
            std::distance(items[index].candidates.begin(), candidate));
        for (const auto& cell : items[index].current_cells) {
            if (cell.tile == FurniturePlacementTile::Support &&
                RoomProvidesFloorSupport(room, cell.x, cell.y)) {
                ++state.room_support_count;
            }
        }
        ExpandBounds(state, items[index].current_cells);
    }
    return state;
}

std::optional<PackState> PackAttachmentsOnCurrentBases(
    const RoomCollisionGrid& room,
    const std::vector<LayoutItem>& items) {
    const auto current = CurrentPackState(room, items);
    if (!current) {
        return std::nullopt;
    }

    PackState state;
    state.occupancy = current->occupancy;
    state.candidate_by_item.resize(items.size());
    std::vector<std::size_t> attachments;
    for (std::size_t index = 0; index < items.size(); ++index) {
        if (!items[index].fixed &&
            items[index].solid_count == 0U &&
            items[index].support_count != 0U) {
            Remove(
                room,
                items[index].placement->instance_id,
                items[index].current_cells,
                state.occupancy);
            attachments.push_back(index);
            continue;
        }
        state.candidate_by_item[index] =
            current->candidate_by_item[index];
        for (const auto& cell : items[index].current_cells) {
            if (cell.tile == FurniturePlacementTile::Support &&
                RoomProvidesFloorSupport(room, cell.x, cell.y)) {
                ++state.room_support_count;
            }
        }
        ExpandBounds(state, items[index].current_cells);
    }
    std::ranges::sort(
        attachments,
        [&items](std::size_t left, std::size_t right) {
            return items[left].placement->instance_id <
                items[right].placement->instance_id;
        });
    for (const auto index : attachments) {
        std::optional<PackState> best;
        for (std::size_t candidate_index = 0;
             candidate_index < items[index].candidates.size();
             ++candidate_index) {
            const auto& cells = items[index].candidates[candidate_index];
            if (!CanPlace(room, state.occupancy, cells)) {
                continue;
            }
            auto next = state;
            next.candidate_by_item[index] = candidate_index;
            for (const auto& cell : cells) {
                if (cell.tile == FurniturePlacementTile::Support &&
                    RoomProvidesFloorSupport(room, cell.x, cell.y)) {
                    ++next.room_support_count;
                }
            }
            Place(
                room,
                items[index].placement->instance_id,
                cells,
                next.occupancy);
            ExpandBounds(next, cells);
            if (!best || BetterPackState(next, *best)) {
                best = std::move(next);
            }
        }
        if (!best) {
            return std::nullopt;
        }
        state = std::move(*best);
    }
    return state;
}

bool CurrentAttachmentsAreAlreadyFurnitureAnchored(
    const RoomCollisionGrid& room,
    const std::vector<LayoutItem>& items) {
    std::size_t attachment_count{};
    for (const auto& item : items) {
        if (item.fixed || item.solid_count != 0U ||
            item.support_count == 0U) {
            continue;
        }
        ++attachment_count;
        for (const auto& cell : item.current_cells) {
            if (cell.tile == FurniturePlacementTile::Support &&
                RoomProvidesFloorSupport(room, cell.x, cell.y)) {
                return false;
            }
        }
    }
    return attachment_count >= 2U;
}

bool CandidateAvoidsFinalTargets(
    const RoomCollisionGrid& room,
    std::size_t item_index,
    const std::vector<MappedCell>& candidate,
    const std::vector<std::vector<MappedCell>>& final_cells) {
    std::map<std::size_t, FurniturePlacementTile> written;
    for (const auto& cell : candidate) {
        if (cell.tile == FurniturePlacementTile::Hitbox ||
            cell.tile == FurniturePlacementTile::Solid ||
            cell.tile == FurniturePlacementTile::PoopLogic) {
            written.emplace(CellIndex(room, cell.x, cell.y), cell.tile);
        }
    }
    for (std::size_t index = 0; index < final_cells.size(); ++index) {
        if (index == item_index) {
            continue;
        }
        for (const auto& cell : final_cells[index]) {
            const auto existing = written.find(
                CellIndex(room, cell.x, cell.y));
            if (existing == written.end()) {
                continue;
            }
            const bool final_body =
                cell.tile == FurniturePlacementTile::Hitbox ||
                cell.tile == FurniturePlacementTile::Solid;
            const bool final_poop =
                cell.tile == FurniturePlacementTile::PoopLogic;
            if (final_body ||
                (final_poop &&
                 (existing->second == FurniturePlacementTile::Solid ||
                  existing->second == FurniturePlacementTile::PoopLogic))) {
                return false;
            }
        }
    }
    return true;
}

ExecutionPlanResult AppendDirectExecutionMoves(
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
    if (!BuildOccupancy(
            room, items, current_cells, current_occupancy, true)) {
        return ExecutionPlanResult::CurrentStateInvalid;
    }
    if (!BuildOccupancy(
            room, items, final_cells, final_occupancy, false)) {
        return ExecutionPlanResult::FinalStateInvalid;
    }

    std::vector<std::pair<std::int32_t, std::int32_t>> current_origins;
    std::vector<std::pair<std::int32_t, std::int32_t>> final_origins;
    std::vector<bool> pending(items.size());
    std::vector<bool> moved(items.size());
    current_origins.reserve(items.size());
    final_origins.reserve(items.size());
    for (std::size_t index = 0; index < items.size(); ++index) {
        current_origins.push_back({
            items[index].placement->position_x,
            items[index].placement->position_y});
        final_origins.push_back(OriginOf(items[index], final_cells[index]));
        pending[index] = current_origins.back() != final_origins.back();
    }

    std::size_t iterations{};
    const auto max_iterations = items.size() * items.size() + 32U;
    while (std::ranges::any_of(pending, [](bool value) { return value; })) {
        if (++iterations > max_iterations) {
            return ExecutionPlanResult::InstallationBlocked;
        }
        bool progressed{};
        for (std::size_t index = 0; index < items.size(); ++index) {
            if (!pending[index]) {
                continue;
            }
            auto trial = current_occupancy;
            Remove(
                room,
                items[index].placement->instance_id,
                current_cells[index],
                trial);
            bool preserves_support = true;
            for (std::size_t dependent = 0;
                 dependent < items.size() && preserves_support;
                 ++dependent) {
                if (dependent == index) {
                    continue;
                }
                for (const auto& cell : current_cells[dependent]) {
                    if (cell.tile == FurniturePlacementTile::Support &&
                        !RoomProvidesBoundarySupport(
                            room, cell.x, cell.y) &&
                        trial.solid_count[
                            CellIndex(room, cell.x, cell.y)] == 0U) {
                        preserves_support = false;
                        break;
                    }
                }
            }
            if (!preserves_support ||
                !CanPlace(room, trial, final_cells[index])) {
                continue;
            }
            const auto from = current_origins[index];
            const auto target = final_origins[index];
            moves.push_back({
                static_cast<std::uint64_t>(
                    items[index].placement->instance_id),
                items[index].placement->item_id,
                room_id,
                room_id,
                from.first,
                from.second,
                target.first,
                target.second});
            current_occupancy = std::move(trial);
            Place(
                room,
                items[index].placement->instance_id,
                final_cells[index],
                current_occupancy);
            current_cells[index] = final_cells[index];
            current_origins[index] = target;
            pending[index] = false;
            moved[index] = true;
            progressed = true;
            break;
        }
        if (!progressed) {
            return ExecutionPlanResult::InstallationBlocked;
        }
    }
    for (std::size_t index = 0; index < items.size(); ++index) {
        if (!moved[index] && !items[index].fixed) {
            ++kept_count;
        }
    }
    return ExecutionPlanResult::Success;
}

ExecutionPlanResult AppendExecutionMoves(
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
    if (!BuildOccupancy(
            room, items, current_cells, current_occupancy, true)) {
        return ExecutionPlanResult::CurrentStateInvalid;
    }
    if (!BuildOccupancy(
            room, items, final_cells, final_occupancy, false)) {
        return ExecutionPlanResult::FinalStateInvalid;
    }
    std::vector<std::pair<std::int32_t, std::int32_t>> current_origins;
    std::vector<std::pair<std::int32_t, std::int32_t>> final_origins;
    std::vector<bool> pending(items.size());
    std::vector<bool> moved(items.size());
    std::vector<bool> forced_staging(items.size());
    current_origins.reserve(items.size());
    final_origins.reserve(items.size());
    for (std::size_t index = 0; index < items.size(); ++index) {
        current_origins.push_back({
            items[index].placement->position_x,
            items[index].placement->position_y});
        final_origins.push_back(OriginOf(items[index], final_cells[index]));
        pending[index] = current_origins.back() != final_origins.back();
    }
    std::vector<bool> evacuated(items.size());
    for (std::size_t index = 0; index < items.size(); ++index) {
        evacuated[index] = items[index].fixed || !pending[index];
    }

    std::size_t evacuation_iterations{};
    const auto max_execution_iterations =
        items.size() * items.size() * 4U + 64U;
    while (std::ranges::any_of(
        evacuated, [](bool value) { return !value; })) {
        if (++evacuation_iterations > max_execution_iterations) {
            return ExecutionPlanResult::EvacuationBlocked;
        }
        bool progressed{};
        for (std::size_t index = 0; index < items.size(); ++index) {
            if (evacuated[index]) {
                continue;
            }
            auto trial_without = current_occupancy;
            Remove(
                room,
                items[index].placement->instance_id,
                current_cells[index],
                trial_without);
            bool preserves_support = true;
            bool reactivated_dependent = false;
            for (std::size_t dependent = 0;
                 dependent < items.size() && preserves_support;
                 ++dependent) {
                if (dependent == index) {
                    continue;
                }
                for (const auto& cell : current_cells[dependent]) {
                    if (cell.tile == FurniturePlacementTile::Support &&
                        !RoomProvidesBoundarySupport(
                            room, cell.x, cell.y) &&
                        trial_without.solid_count[
                            CellIndex(room, cell.x, cell.y)] == 0U) {
                        preserves_support = false;
                        if (!items[dependent].fixed &&
                            evacuated[dependent]) {
                            evacuated[dependent] = false;
                            pending[dependent] = true;
                            forced_staging[dependent] = true;
                            reactivated_dependent = true;
                        }
                        break;
                    }
                }
            }
            if (!preserves_support) {
                if (reactivated_dependent) {
                    progressed = true;
                    break;
                }
                continue;
            }
            const auto room_anchored = [&room](
                const std::vector<MappedCell>& cells) {
                return std::ranges::all_of(
                    cells,
                    [&room](const auto& cell) {
                        return cell.tile !=
                                FurniturePlacementTile::Support ||
                            RoomProvidesFloorSupport(
                                room, cell.x, cell.y);
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

            if (!forced_staging[index] &&
                CanPlace(room, trial_without, final_cells[index])) {
                const auto from = current_origins[index];
                const auto target = final_origins[index];
                if (from != target) {
                    moves.push_back({
                        static_cast<std::uint64_t>(
                            items[index].placement->instance_id),
                        items[index].placement->item_id,
                        room_id,
                        room_id,
                        from.first,
                        from.second,
                        target.first,
                        target.second});
                    moved[index] = true;
                }
                current_occupancy = trial_without;
                Place(
                    room,
                    items[index].placement->instance_id,
                    final_cells[index],
                    current_occupancy);
                current_cells[index] = final_cells[index];
                current_origins[index] = target;
                pending[index] = false;
                evacuated[index] = true;
                progressed = true;
                break;
            }

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
            pending[index] = true;
            moved[index] = true;
            forced_staging[index] = false;
            evacuated[index] = true;
            progressed = true;
            break;
        }
        if (!progressed) {
            return ExecutionPlanResult::EvacuationBlocked;
        }
    }

    std::size_t installation_iterations{};
    while (std::ranges::any_of(pending, [](bool value) { return value; })) {
        if (++installation_iterations > max_execution_iterations) {
            return ExecutionPlanResult::InstallationBlocked;
        }
        bool progressed{};
        for (std::size_t index = 0; index < items.size(); ++index) {
            if (!pending[index]) {
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
                    room_id,
                    from.first,
                    from.second,
                    target.first,
                    target.second});
                moved[index] = true;
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
            return ExecutionPlanResult::InstallationBlocked;
        }
    }
    for (std::size_t index = 0; index < items.size(); ++index) {
        if (!moved[index] && !items[index].fixed) {
            ++kept_count;
        }
    }
    return ExecutionPlanResult::Success;
}

std::optional<RoomCollisionGrid> ResolveRuntimeRoom(
    const snapshot::detail::HouseGeometryCatalog& geometry,
    const FurnitureRoomGrid& runtime) {
    const auto cell_count = runtime.width * runtime.height;
    const bool has_runtime_cells =
        !runtime.base_cells.empty() || !runtime.live_cells.empty();
    if (has_runtime_cells) {
        if (runtime.width == 0U || runtime.height == 0U ||
            runtime.base_cells.size() != cell_count ||
            runtime.live_cells.size() != cell_count) {
            return std::nullopt;
        }
        return RoomCollisionGrid{
            .supported = true,
            .width = runtime.width,
            .height = runtime.height,
            .cells = runtime.base_cells};
    }
    for (const auto& definition : geometry.rooms) {
        if (definition.room_id != runtime.room_id) {
            continue;
        }
        auto decoded = snapshot::detail::DecodeRoomCollisionGrid(definition);
        if (decoded.supported && decoded.width == runtime.width &&
            decoded.height == runtime.height) {
            return decoded;
        }
    }
    return std::nullopt;
}

std::optional<std::uint8_t> CommittedGridValue(
    FurniturePlacementTile tile) {
    if (tile == FurniturePlacementTile::Hitbox) {
        return static_cast<std::uint8_t>(1U);
    }
    if (tile == FurniturePlacementTile::Solid) {
        return static_cast<std::uint8_t>(2U);
    }
    if (tile == FurniturePlacementTile::PoopLogic) {
        return static_cast<std::uint8_t>(5U);
    }
    return std::nullopt;
}

bool NativeWriterAllowedOnBase(
    std::uint8_t writer,
    std::uint8_t base) {
    if (RoomBlocksAllFurniture(base)) {
        return false;
    }
    if (writer == 1U || writer == 2U) {
        return base != 1U && base != 2U && base != 5U;
    }
    return writer == 5U && base != 2U && base != 5U;
}

void ApplyRuntimeLiveGridEvidence(
    std::map<snapshot::RoomId, RoomCollisionGrid>& rooms,
    const std::vector<FurnitureRoomGrid>& runtime_room_grids,
    const std::vector<FurniturePlacement>& furniture,
    const std::unordered_map<
        std::string, const FurnitureInfoRecord*>& info_by_item,
    std::set<snapshot::RoomId>& unsafe_rooms) {
    for (const auto& runtime : runtime_room_grids) {
        if (runtime.base_cells.empty() && runtime.live_cells.empty()) {
            continue;
        }
        const auto room = rooms.find(runtime.room_id);
        const auto cell_count = runtime.width * runtime.height;
        if (room == rooms.end() || runtime.width == 0U ||
            runtime.height == 0U ||
            runtime.base_cells.size() != cell_count ||
            runtime.live_cells.size() != cell_count) {
            unsafe_rooms.insert(runtime.room_id);
            continue;
        }
        std::vector<std::uint8_t> known_writer(cell_count, 0U);
        std::vector<std::uint16_t> known_writer_count(cell_count, 0U);
        for (const auto& placement : furniture) {
            if (placement.room_id != runtime.room_id) {
                continue;
            }
            const auto info = info_by_item.find(placement.item_id);
            if (info == info_by_item.end()) {
                continue;
            }
            const auto offsets = ActiveOffsets(placement, *info->second);
            std::vector<MappedCell> cells;
            if (offsets.empty() || !MapCells(placement, offsets, cells)) {
                continue;
            }
            for (const auto& cell : cells) {
                const auto writer = CommittedGridValue(cell.tile);
                if (!writer || !Inside(room->second, cell.x, cell.y)) {
                    continue;
                }
                const auto index = CellIndex(room->second, cell.x, cell.y);
                known_writer[index] = *writer;
                ++known_writer_count[index];
            }
        }
        bool consistent = true;
        for (std::size_t index = 0; index < cell_count; ++index) {
            const auto base = runtime.base_cells[index];
            const auto live = runtime.live_cells[index];
            if (known_writer_count[index] != 0U) {
                if (known_writer_count[index] != 1U ||
                    !NativeWriterAllowedOnBase(
                        known_writer[index], base) ||
                    live != known_writer[index]) {
                    consistent = false;
                }
                continue;
            }
            if (live == base) {
                continue;
            }
            if (live == 1U || live == 2U || live == 5U) {
                room->second.cells[index] = live;
            } else {
                consistent = false;
            }
        }
        if (!consistent) {
            unsafe_rooms.insert(runtime.room_id);
        }
    }
}

bool RoomAnchored(
    const RoomCollisionGrid& room,
    const std::vector<MappedCell>& cells) {
    return std::ranges::all_of(cells, [&room](const auto& cell) {
        return cell.tile != FurniturePlacementTile::Support ||
            RoomProvidesBoundarySupport(room, cell.x, cell.y);
    });
}

ExecutionPlanResult AppendWholeHouseExecutionMoves(
    const std::map<snapshot::RoomId, RoomCollisionGrid>& rooms,
    const std::set<snapshot::RoomId>& staging_forbidden_rooms,
    const snapshot::RoomId& target_room_id,
    const std::vector<LayoutItem>& items,
    const std::vector<LayoutItem>& static_items,
    const std::vector<std::vector<MappedCell>>& final_cells,
    std::vector<FurnitureLayoutMove>& moves,
    std::size_t& kept_count) {
    const auto target_entry = rooms.find(target_room_id);
    if (target_entry == rooms.end()) {
        return ExecutionPlanResult::FinalStateInvalid;
    }
    const auto& target_room = target_entry->second;
    Occupancy final_occupancy;
    if (!BuildOccupancy(
            target_room, items, final_cells, final_occupancy, false)) {
        return ExecutionPlanResult::FinalStateInvalid;
    }
    for (const auto& item : static_items) {
        if (item.placement->room_id != target_room_id) {
            continue;
        }
        if (!CanKeepExistingPlacement(
                target_room, final_occupancy, item.current_cells)) {
            return ExecutionPlanResult::FinalStateInvalid;
        }
        Place(
            target_room,
            item.placement->instance_id,
            item.current_cells,
            final_occupancy);
    }

    std::map<snapshot::RoomId, Occupancy> occupancies;
    for (const auto& [room_id, room] : rooms) {
        auto& occupancy = occupancies[room_id];
        occupancy.hitbox_count.assign(room.width * room.height, 0U);
        occupancy.solid_count.assign(room.width * room.height, 0U);
        occupancy.poop_count.assign(room.width * room.height, 0U);
    }
    for (const auto& item : items) {
        const auto room = rooms.find(item.placement->room_id);
        if (room == rooms.end() ||
            !std::ranges::all_of(
                item.current_cells,
                [&room](const auto& cell) {
                    return Inside(room->second, cell.x, cell.y);
                })) {
            return ExecutionPlanResult::CurrentStateInvalid;
        }
        Place(
            room->second,
            item.placement->instance_id,
            item.current_cells,
            occupancies.at(item.placement->room_id));
    }
    for (const auto& item : static_items) {
        const auto room = rooms.find(item.placement->room_id);
        if (room == rooms.end() ||
            !std::ranges::all_of(
                item.current_cells,
                [&room](const auto& cell) {
                    return Inside(room->second, cell.x, cell.y);
                })) {
            return ExecutionPlanResult::CurrentStateInvalid;
        }
        Place(
            room->second,
            item.placement->instance_id,
            item.current_cells,
            occupancies.at(item.placement->room_id));
    }

    std::vector<snapshot::RoomId> current_rooms;
    std::vector<std::vector<MappedCell>> current_cells;
    std::vector<std::pair<std::int32_t, std::int32_t>> current_origins;
    std::vector<std::pair<std::int32_t, std::int32_t>> final_origins;
    std::vector<bool> pending(items.size());
    std::vector<bool> evacuated(items.size());
    std::vector<bool> moved(items.size());
    current_rooms.reserve(items.size());
    current_cells.reserve(items.size());
    current_origins.reserve(items.size());
    final_origins.reserve(items.size());
    for (std::size_t index = 0; index < items.size(); ++index) {
        current_rooms.push_back(items[index].placement->room_id);
        current_cells.push_back(items[index].current_cells);
        current_origins.push_back({
            items[index].placement->position_x,
            items[index].placement->position_y});
        final_origins.push_back(OriginOf(items[index], final_cells[index]));
        pending[index] = current_rooms[index] != target_room_id ||
            current_origins[index] != final_origins[index];
        evacuated[index] = !pending[index];
    }

    const auto max_iterations = items.size() * items.size() * 8U + 128U;
    std::size_t iterations{};
    while (std::ranges::any_of(
        evacuated, [](bool value) { return !value; })) {
        if (++iterations > max_iterations) {
            return ExecutionPlanResult::EvacuationBlocked;
        }
        bool progressed{};
        for (std::size_t index = 0; index < items.size(); ++index) {
            if (evacuated[index]) {
                continue;
            }
            const auto source_entry = rooms.find(current_rooms[index]);
            if (source_entry == rooms.end()) {
                return ExecutionPlanResult::CurrentStateInvalid;
            }
            const auto& source_room = source_entry->second;
            auto source_without = occupancies.at(current_rooms[index]);
            Remove(
                source_room,
                items[index].placement->instance_id,
                current_cells[index],
                source_without);
            bool preserves_support = true;
            for (std::size_t dependent = 0;
                 dependent < items.size() && preserves_support;
                 ++dependent) {
                if (dependent == index ||
                    current_rooms[dependent] != current_rooms[index]) {
                    continue;
                }
                for (const auto& cell : current_cells[dependent]) {
                    if (cell.tile == FurniturePlacementTile::Support &&
                        !RoomProvidesBoundarySupport(
                            source_room, cell.x, cell.y) &&
                        source_without.solid_count[
                            CellIndex(source_room, cell.x, cell.y)] == 0U) {
                        preserves_support = false;
                        break;
                    }
                }
            }
            for (const auto& dependent : static_items) {
                if (!preserves_support ||
                    dependent.placement->room_id != current_rooms[index]) {
                    continue;
                }
                for (const auto& cell : dependent.current_cells) {
                    if (cell.tile == FurniturePlacementTile::Support &&
                        !RoomProvidesBoundarySupport(
                            source_room, cell.x, cell.y) &&
                        source_without.solid_count[
                            CellIndex(source_room, cell.x, cell.y)] == 0U) {
                        preserves_support = false;
                        break;
                    }
                }
            }
            if (!preserves_support) {
                continue;
            }

            auto destination = current_rooms[index] == target_room_id
                ? source_without
                : occupancies.at(target_room_id);
            const auto from_room = current_rooms[index];
            const auto from = current_origins[index];
            const auto final = final_origins[index];
            if (CanPlace(target_room, destination, final_cells[index])) {
                moves.push_back({
                    static_cast<std::uint64_t>(
                        items[index].placement->instance_id),
                    items[index].placement->item_id,
                    from_room,
                    target_room_id,
                    from.first,
                    from.second,
                    final.first,
                    final.second});
                occupancies.at(from_room) = std::move(source_without);
                Place(
                    target_room,
                    items[index].placement->instance_id,
                    final_cells[index],
                    destination);
                occupancies.at(target_room_id) = std::move(destination);
                current_rooms[index] = target_room_id;
                current_cells[index] = final_cells[index];
                current_origins[index] = final;
                pending[index] = false;
                evacuated[index] = true;
                moved[index] = true;
                progressed = true;
                break;
            }

            struct StagingCandidate {
                snapshot::RoomId room_id;
                std::vector<MappedCell> cells;
                std::tuple<bool, std::int64_t, snapshot::RoomId> score;
            };
            std::optional<StagingCandidate> staging;
            for (const auto& [buffer_room_id, buffer_room] : rooms) {
                if (staging_forbidden_rooms.contains(buffer_room_id)) {
                    continue;
                }
                auto buffer_occupancy = buffer_room_id == from_room
                    ? source_without
                    : occupancies.at(buffer_room_id);
                const auto candidates = GenerateCandidates(
                    buffer_room, items[index]);
                for (const auto& candidate : candidates) {
                    const auto origin = OriginOf(items[index], candidate);
                    if ((buffer_room_id == target_room_id && origin == final) ||
                        (buffer_room_id == from_room && origin == from) ||
                        !RoomAnchored(buffer_room, candidate) ||
                        (buffer_room_id == target_room_id &&
                         !CandidateAvoidsFinalTargets(
                             target_room, index, candidate, final_cells)) ||
                        !CanPlace(
                            buffer_room, buffer_occupancy, candidate)) {
                        continue;
                    }
                    std::int64_t coordinate_score{};
                    for (const auto& cell : candidate) {
                        coordinate_score +=
                            static_cast<std::int64_t>(cell.x) + cell.y;
                    }
                    StagingCandidate next{
                        buffer_room_id,
                        candidate,
                        {buffer_room_id != target_room_id,
                         coordinate_score,
                         buffer_room_id}};
                    if (!staging || next.score < staging->score) {
                        staging = std::move(next);
                    }
                }
            }
            if (!staging) {
                continue;
            }
            const auto& candidate = staging->cells;
            const auto target = OriginOf(items[index], candidate);
            moves.push_back({
                static_cast<std::uint64_t>(
                    items[index].placement->instance_id),
                items[index].placement->item_id,
                from_room,
                staging->room_id,
                from.first,
                from.second,
                target.first,
                target.second});
            occupancies.at(from_room) = std::move(source_without);
            auto staged_occupancy = staging->room_id == from_room
                ? occupancies.at(from_room)
                : occupancies.at(staging->room_id);
            Place(
                rooms.at(staging->room_id),
                items[index].placement->instance_id,
                candidate,
                staged_occupancy);
            occupancies.at(staging->room_id) =
                std::move(staged_occupancy);
            current_rooms[index] = staging->room_id;
            current_cells[index] = candidate;
            current_origins[index] = target;
            pending[index] = true;
            evacuated[index] = true;
            moved[index] = true;
            progressed = true;
            break;
        }
        if (!progressed) {
            return ExecutionPlanResult::EvacuationBlocked;
        }
    }

    iterations = 0U;
    while (std::ranges::any_of(pending, [](bool value) { return value; })) {
        if (++iterations > max_iterations) {
            return ExecutionPlanResult::InstallationBlocked;
        }
        bool progressed{};
        for (std::size_t index = 0; index < items.size(); ++index) {
            if (!pending[index]) {
                continue;
            }
            const auto from_room = current_rooms[index];
            const auto& source_room = rooms.at(from_room);
            auto source_without = occupancies.at(from_room);
            Remove(
                source_room,
                items[index].placement->instance_id,
                current_cells[index],
                source_without);
            auto destination = from_room == target_room_id
                ? source_without
                : occupancies.at(target_room_id);
            if (!CanPlace(
                    target_room, destination, final_cells[index])) {
                continue;
            }
            const auto from = current_origins[index];
            const auto target = final_origins[index];
            moves.push_back({
                static_cast<std::uint64_t>(
                    items[index].placement->instance_id),
                items[index].placement->item_id,
                from_room,
                target_room_id,
                from.first,
                from.second,
                target.first,
                target.second});
            Place(
                target_room,
                items[index].placement->instance_id,
                final_cells[index],
                destination);
            occupancies.at(from_room) = std::move(source_without);
            occupancies.at(target_room_id) = std::move(destination);
            current_rooms[index] = target_room_id;
            current_cells[index] = final_cells[index];
            current_origins[index] = target;
            pending[index] = false;
            moved[index] = true;
            progressed = true;
            break;
        }
        if (!progressed) {
            return ExecutionPlanResult::InstallationBlocked;
        }
    }
    for (std::size_t index = 0; index < items.size(); ++index) {
        if (!moved[index]) {
            ++kept_count;
        }
    }
    return ExecutionPlanResult::Success;
}

FurnitureLayoutPlan PlanWholeHouse(
    const std::vector<FurniturePlacement>& furniture,
    const snapshot::detail::HouseGeometryCatalog& geometry,
    const snapshot::detail::FurnitureInfoCatalog& furniture_info,
    const std::vector<FurnitureRoomGrid>& runtime_room_grids,
    const std::vector<snapshot::RoomId>& locked_room_ids) {
    FurnitureLayoutPlan plan;
    const std::set<snapshot::RoomId> locked_rooms(
        locked_room_ids.begin(), locked_room_ids.end());
    std::map<snapshot::RoomId, RoomCollisionGrid> rooms;
    for (const auto& runtime : runtime_room_grids) {
        if (runtime.room_id.empty() || runtime.room_id == "AdventureBox") {
            continue;
        }
        if (locked_rooms.contains(runtime.room_id)) {
            continue;
        }
        const auto resolved = ResolveRuntimeRoom(geometry, runtime);
        if (resolved) {
            rooms.emplace(runtime.room_id, *resolved);
        }
    }
    if (rooms.empty()) {
        plan.unsupported_furniture_count = furniture.size();
        return plan;
    }

    std::unordered_map<std::string, const FurnitureInfoRecord*> info_by_item;
    for (const auto& info : furniture_info.records) {
        info_by_item.emplace(info.item_id, &info);
    }
    std::set<snapshot::RoomId> unsafe_rooms;
    ApplyRuntimeLiveGridEvidence(
        rooms,
        runtime_room_grids,
        furniture,
        info_by_item,
        unsafe_rooms);
    std::vector<LayoutItem> movable_items;
    movable_items.reserve(furniture.size());
    std::vector<LayoutItem> fixed_items;
    fixed_items.reserve(furniture.size());
    for (const auto& placement : furniture) {
        if (placement.room_id.empty()) {
            ++plan.warehouse_furniture_count;
            continue;
        }
        if (locked_rooms.contains(placement.room_id)) {
            ++plan.kept_furniture_count;
            continue;
        }
        if (unsafe_rooms.contains(placement.room_id)) {
            ++plan.unsupported_furniture_count;
            continue;
        }
        const auto source_room = rooms.find(placement.room_id);
        const auto info = info_by_item.find(placement.item_id);
        if (placement.instance_id <= 0 || source_room == rooms.end() ||
            info == info_by_item.end()) {
            ++plan.unsupported_furniture_count;
            if (source_room != rooms.end()) {
                unsafe_rooms.insert(placement.room_id);
            }
            continue;
        }
        LayoutItem item;
        item.placement = &placement;
        item.offsets = ActiveOffsets(placement, *info->second);
        if (item.offsets.empty()) {
            ++plan.unsupported_furniture_count;
            item.offsets = RecognizedOffsets(placement, *info->second);
            if (!item.offsets.empty() &&
                MapCells(placement, item.offsets, item.current_cells)) {
                std::erase_if(
                    item.current_cells,
                    [&source_room](const auto& cell) {
                        return !Inside(
                            source_room->second, cell.x, cell.y);
                    });
                if (!item.current_cells.empty()) {
                    fixed_items.push_back(std::move(item));
                }
            }
            continue;
        }
        if (!MapCells(placement, item.offsets, item.current_cells)) {
            ++plan.unsupported_furniture_count;
            unsafe_rooms.insert(placement.room_id);
            continue;
        }
        if (!std::ranges::all_of(
                item.current_cells,
                [&source_room](const auto& cell) {
                    return Inside(source_room->second, cell.x, cell.y);
                })) {
            ++plan.unsupported_furniture_count;
            std::erase_if(
                item.current_cells,
                [&source_room](const auto& cell) {
                    return !Inside(source_room->second, cell.x, cell.y);
                });
            if (!item.current_cells.empty()) {
                fixed_items.push_back(std::move(item));
            }
            continue;
        }
        item.hitbox_count = static_cast<std::size_t>(std::ranges::count_if(
            item.offsets,
            [](const auto& cell) {
                return cell.tile == FurniturePlacementTile::Hitbox;
            }));
        item.solid_count = static_cast<std::size_t>(std::ranges::count_if(
            item.offsets,
            [](const auto& cell) {
                return cell.tile == FurniturePlacementTile::Solid;
            }));
        item.surface_count = static_cast<std::size_t>(std::ranges::count_if(
            item.offsets,
            [](const auto& cell) {
                return cell.tile == FurniturePlacementTile::Surface;
            }));
        item.support_count = static_cast<std::size_t>(std::ranges::count_if(
            item.offsets,
            [](const auto& cell) {
                return cell.tile == FurniturePlacementTile::Support;
            }));
        item.poop_count = static_cast<std::size_t>(std::ranges::count_if(
            item.offsets,
            [](const auto& cell) {
                return cell.tile == FurniturePlacementTile::PoopLogic;
            }));
        ++plan.considered_furniture_count;
        if (item.hitbox_count == 0U && item.solid_count == 0U &&
            item.support_count == 0U && item.poop_count == 0U) {
            ++plan.kept_furniture_count;
            continue;
        }
        movable_items.push_back(std::move(item));
    }
    if (movable_items.empty()) {
        return plan;
    }

    std::vector<LayoutItem> eligible_items;
    eligible_items.reserve(movable_items.size());
    for (auto& item : movable_items) {
        eligible_items.push_back(std::move(item));
    }
    if (eligible_items.empty()) {
        return plan;
    }

    std::vector<snapshot::RoomId> target_rooms;
    target_rooms.reserve(rooms.size());
    for (const auto& [room_id, room] : rooms) {
        (void)room;
        if (!unsafe_rooms.contains(room_id)) {
            target_rooms.push_back(room_id);
        }
    }
    std::ranges::sort(
        target_rooms,
        [&rooms](const auto& left, const auto& right) {
            const auto& left_room = rooms.at(left);
            const auto& right_room = rooms.at(right);
            const auto left_area = left_room.width * left_room.height;
            const auto right_area = right_room.width * right_room.height;
            if (left_area != right_area) {
                return left_area > right_area;
            }
            if ((left == "Attic") != (right == "Attic")) {
                return left == "Attic";
            }
            return left < right;
        });

    bool evacuation_blocked{};
    bool installation_blocked{};
    bool found_geometric_candidate{};
    for (const auto& target_room_id : target_rooms) {
        const auto& target_room = rooms.at(target_room_id);
        std::vector<LayoutItem> target_items;
        std::vector<LayoutItem> target_static_base = fixed_items;
        std::size_t target_unplaceable_movable_count{};
        target_items.reserve(eligible_items.size());
        for (const auto& source_item : eligible_items) {
            auto item = source_item;
            item.candidates = GenerateCandidates(target_room, item);
            if (item.candidates.empty()) {
                target_static_base.push_back(std::move(item));
                ++target_unplaceable_movable_count;
            } else {
                target_items.push_back(std::move(item));
            }
        }
        if (target_items.empty()) {
            continue;
        }
        std::vector<LayoutItem> target_fixed_items;
        for (const auto& item : target_static_base) {
            if (item.placement->room_id == target_room_id) {
                target_fixed_items.push_back(item);
            }
        }
        std::vector<bool> required(target_items.size());
        for (std::size_t index = 0; index < target_items.size(); ++index) {
            required[index] =
                target_items[index].placement->room_id == target_room_id;
        }

        std::vector<PackState> packed_candidates;
        if (std::ranges::all_of(required, [](bool value) { return value; })) {
            const auto current = CurrentSelectedPackState(
                target_room, target_items, target_fixed_items);
            if (current) {
                packed_candidates.push_back(*current);
            }
        }
        const auto anchor_chain_seeds = BuildAnchorChainSeeds(
            target_room,
            target_items,
            target_fixed_items,
            required);
        for (const auto& order : PackingOrders(target_items)) {
            auto packed = PackSubsetInOrder(
                target_room,
                target_items,
                target_fixed_items,
                required,
                order);
            packed_candidates.insert(
                packed_candidates.end(),
                std::make_move_iterator(packed.begin()),
                std::make_move_iterator(packed.end()));
            if (!anchor_chain_seeds.empty()) {
                auto anchored = PackSubsetInOrder(
                    target_room,
                    target_items,
                    target_fixed_items,
                    required,
                    order,
                    &anchor_chain_seeds);
                packed_candidates.insert(
                    packed_candidates.end(),
                    std::make_move_iterator(anchored.begin()),
                    std::make_move_iterator(anchored.end()));
            }
        }
        std::ranges::sort(packed_candidates, BetterFilledPackState);
        const auto unique_end = std::ranges::unique(
            packed_candidates, {}, &PackState::candidate_by_item);
        packed_candidates.erase(unique_end.begin(), unique_end.end());
        for (const auto& candidate : packed_candidates) {
            if (candidate.selected_count == 0U) {
                continue;
            }
            found_geometric_candidate = true;
            std::vector<LayoutItem> selected_items;
            std::vector<LayoutItem> static_items;
            std::vector<std::vector<MappedCell>> final_cells;
            selected_items.reserve(candidate.selected_count);
            static_items.reserve(
                target_static_base.size() +
                target_items.size() - candidate.selected_count);
            static_items.insert(
                static_items.end(),
                target_static_base.begin(),
                target_static_base.end());
            final_cells.reserve(candidate.selected_count);
            for (std::size_t index = 0;
                 index < target_items.size();
                 ++index) {
                if (!candidate.candidate_by_item[index]) {
                    static_items.push_back(target_items[index]);
                    continue;
                }
                selected_items.push_back(target_items[index]);
                final_cells.push_back(
                    target_items[index].candidates[
                        *candidate.candidate_by_item[index]]);
            }
            std::vector<FurnitureLayoutMove> candidate_moves;
            auto kept = plan.kept_furniture_count;
            const auto execution = AppendWholeHouseExecutionMoves(
                rooms,
                unsafe_rooms,
                target_room_id,
                selected_items,
                static_items,
                final_cells,
                candidate_moves,
                kept);
            if (execution == ExecutionPlanResult::Success) {
                plan.kept_furniture_count = kept;
                plan.deferred_furniture_count =
                    target_unplaceable_movable_count +
                    target_items.size() - candidate.selected_count;
                plan.moves = std::move(candidate_moves);
                plan.target_room_id = target_room_id;
                plan.planned_room_count = 1U;
                return plan;
            }
            evacuation_blocked = evacuation_blocked ||
                execution == ExecutionPlanResult::EvacuationBlocked;
            installation_blocked = installation_blocked ||
                execution == ExecutionPlanResult::InstallationBlocked;
        }
    }
    plan.evacuation_blocked_room_count = evacuation_blocked ? 1U : 0U;
    plan.installation_blocked_room_count = installation_blocked ? 1U : 0U;
    if (!found_geometric_candidate) {
        plan.no_space_furniture_count += eligible_items.size();
    }
    return plan;
}

}  // namespace

std::optional<std::vector<FurnitureSupportDependent>>
FindFurnitureSupportDependentsTopDown(
    const FurniturePlacement& provider,
    const std::vector<FurniturePlacement>& furniture,
    const snapshot::detail::HouseGeometryCatalog& geometry,
    const snapshot::detail::FurnitureInfoCatalog& furniture_info,
    const std::vector<FurnitureRoomGrid>& runtime_room_grids) {
    if (provider.room_id.empty() || provider.instance_id <= 0) {
        return std::nullopt;
    }
    const auto runtime = std::ranges::find_if(
        runtime_room_grids,
        [&provider](const auto& room) {
            return room.room_id == provider.room_id;
        });
    if (runtime == runtime_room_grids.end()) {
        return std::nullopt;
    }
    const auto resolved = ResolveRuntimeRoom(geometry, *runtime);
    if (!resolved) {
        return std::nullopt;
    }
    const auto& room = *resolved;

    std::unordered_map<std::string, const FurnitureInfoRecord*> info_by_item;
    info_by_item.reserve(furniture_info.records.size());
    for (const auto& info : furniture_info.records) {
        info_by_item.emplace(info.item_id, &info);
    }

    struct DependencyItem {
        const FurniturePlacement* placement{};
        std::vector<std::pair<std::int32_t, std::int32_t>> solids;
        std::vector<std::pair<std::int32_t, std::int32_t>> supports;
    };
    std::vector<DependencyItem> items;
    for (const auto& placement : furniture) {
        if (placement.room_id != provider.room_id ||
            placement.instance_id <= 0) {
            continue;
        }
        const auto info = info_by_item.find(placement.item_id);
        if (info == info_by_item.end() ||
            !placement.HasSupportedGridScale() ||
            !info->second->placement_grid.supported) {
            return std::nullopt;
        }
        const auto offsets = ActiveOffsets(placement, *info->second);
        std::vector<MappedCell> cells;
        if (!MapCells(placement, offsets, cells) ||
            !std::ranges::all_of(cells, [&room](const auto& cell) {
                return Inside(room, cell.x, cell.y);
            })) {
            return std::nullopt;
        }
        DependencyItem item;
        item.placement = &placement;
        for (const auto& cell : cells) {
            if (cell.tile == FurniturePlacementTile::Solid) {
                item.solids.emplace_back(cell.x, cell.y);
            } else if (cell.tile == FurniturePlacementTile::Support &&
                       !RoomProvidesBoundarySupport(room, cell.x, cell.y)) {
                item.supports.emplace_back(cell.x, cell.y);
            }
        }
        std::ranges::sort(item.solids);
        std::ranges::sort(item.supports);
        items.push_back(std::move(item));
    }
    std::ranges::sort(items, [](const auto& left, const auto& right) {
        return left.placement->instance_id < right.placement->instance_id;
    });
    const auto provider_index = std::ranges::find_if(
        items,
        [&provider](const auto& item) {
            return item.placement->instance_id == provider.instance_id;
        });
    if (provider_index == items.end()) {
        return std::nullopt;
    }

    const auto directly_depends_on = [](const auto& dependent,
                                        const auto& supporting) {
        return std::ranges::any_of(
            dependent.supports,
            [&supporting](const auto& support) {
                return std::ranges::binary_search(
                    supporting.solids, support);
            });
    };
    std::vector<std::uint8_t> visit_state(items.size(), 0U);
    std::vector<FurnitureSupportDependent> result;
    bool cycle{};
    const auto visit = [&](const auto& self, std::size_t index) -> void {
        if (cycle || visit_state[index] == 2U) {
            return;
        }
        if (visit_state[index] == 1U) {
            cycle = true;
            return;
        }
        visit_state[index] = 1U;
        for (std::size_t dependent = 0; dependent < items.size(); ++dependent) {
            if (dependent == index ||
                !directly_depends_on(items[dependent], items[index])) {
                continue;
            }
            self(self, dependent);
            if (cycle) {
                return;
            }
            const auto stable_key = static_cast<std::uint64_t>(
                items[dependent].placement->instance_id);
            if (std::ranges::none_of(
                    result,
                    [stable_key](const auto& existing) {
                        return existing.stable_key == stable_key;
                    })) {
                result.push_back({
                    .stable_key = stable_key,
                    .item_id = items[dependent].placement->item_id,
                    .room_id = items[dependent].placement->room_id,
                    .x = items[dependent].placement->position_x,
                    .y = items[dependent].placement->position_y});
            }
        }
        visit_state[index] = 2U;
    };
    visit(
        visit,
        static_cast<std::size_t>(
            std::distance(items.begin(), provider_index)));
    if (cycle) {
        return std::nullopt;
    }
    return result;
}

std::optional<FurnitureReplacementPlacement>
FindNearestFurnitureReplacementPlacement(
    const FurniturePlacement& placed,
    const FurniturePlacement& warehouse,
    const std::vector<FurniturePlacement>& furniture,
    const snapshot::detail::HouseGeometryCatalog& geometry,
    const snapshot::detail::FurnitureInfoCatalog& furniture_info,
    const std::vector<FurnitureRoomGrid>& runtime_room_grids) {
    if (placed.room_id.empty() || placed.instance_id <= 0 ||
        warehouse.instance_id <= 0) {
        return std::nullopt;
    }
    const auto runtime = std::ranges::find_if(
        runtime_room_grids,
        [&placed](const auto& room) {
            return room.room_id == placed.room_id;
        });
    if (runtime == runtime_room_grids.end()) {
        return std::nullopt;
    }
    const auto resolved = ResolveRuntimeRoom(geometry, *runtime);
    if (!resolved) {
        return std::nullopt;
    }
    const auto& room = *resolved;

    std::unordered_map<std::string, const FurnitureInfoRecord*> info_by_item;
    info_by_item.reserve(furniture_info.records.size());
    for (const auto& info : furniture_info.records) {
        info_by_item.emplace(info.item_id, &info);
    }
    const auto replacement_info = info_by_item.find(warehouse.item_id);
    const auto placed_info = info_by_item.find(placed.item_id);
    if (replacement_info == info_by_item.end() ||
        placed_info == info_by_item.end()) {
        return std::nullopt;
    }

    FurniturePlacement replacement = warehouse;
    replacement.room_id = placed.room_id;
    replacement.position_x = placed.position_x;
    replacement.position_y = placed.position_y;
    replacement.position_z = placed.position_z;
    replacement.scale_x = placed.scale_x;
    replacement.scale_y = placed.scale_y;
    LayoutItem replacement_item;
    replacement_item.placement = &replacement;
    replacement_item.offsets = ActiveOffsets(
        replacement, *replacement_info->second);
    if (replacement_item.offsets.empty()) {
        return std::nullopt;
    }
    replacement_item.candidates = GenerateCandidates(room, replacement_item);
    if (replacement_item.candidates.empty()) {
        return std::nullopt;
    }

    Occupancy occupancy;
    occupancy.hitbox_count.assign(room.width * room.height, 0U);
    occupancy.solid_count.assign(room.width * room.height, 0U);
    occupancy.poop_count.assign(room.width * room.height, 0U);
    std::vector<std::vector<MappedCell>> other_cells;
    for (const auto& item : furniture) {
        if (item.room_id != placed.room_id ||
            item.instance_id == placed.instance_id) {
            continue;
        }
        const auto info = info_by_item.find(item.item_id);
        if (info == info_by_item.end()) {
            continue;
        }
        const auto offsets = ActiveOffsets(item, *info->second);
        std::vector<MappedCell> cells;
        if (offsets.empty() || !MapCells(item, offsets, cells) ||
            !std::ranges::all_of(cells, [&room](const auto& cell) {
                return Inside(room, cell.x, cell.y);
            })) {
            continue;
        }
        Place(room, item.instance_id, cells, occupancy);
        other_cells.push_back(std::move(cells));
    }

    const auto placed_offsets = ActiveOffsets(placed, *placed_info->second);
    std::vector<MappedCell> placed_cells;
    std::vector<bool> old_body(room.width * room.height, false);
    if (!placed_offsets.empty() &&
        MapCells(placed, placed_offsets, placed_cells)) {
        for (const auto& cell : placed_cells) {
            if (Inside(room, cell.x, cell.y) &&
                (cell.tile == FurniturePlacementTile::Hitbox ||
                 cell.tile == FurniturePlacementTile::Solid ||
                 cell.tile == FurniturePlacementTile::PoopLogic)) {
                old_body[CellIndex(room, cell.x, cell.y)] = true;
            }
        }
    }
    const auto cell_count = room.width * room.height;
    if (runtime->base_cells.size() == cell_count &&
        runtime->live_cells.size() == cell_count) {
        for (std::size_t index = 0; index < cell_count; ++index) {
            if (old_body[index] || BodyOccupied(occupancy, index) ||
                runtime->live_cells[index] == runtime->base_cells[index]) {
                continue;
            }
            if (runtime->live_cells[index] == 1U) {
                occupancy.hitbox_count[index] = 1U;
            } else if (runtime->live_cells[index] == 2U) {
                occupancy.solid_count[index] = 1U;
            } else if (runtime->live_cells[index] == 5U) {
                occupancy.poop_count[index] = 1U;
            }
        }
    }

    std::optional<FurnitureReplacementPlacement> best;
    std::tuple<std::int64_t, std::int64_t, std::int64_t,
               std::int32_t, std::int32_t> best_rank;
    for (const auto& candidate : replacement_item.candidates) {
        if (!CanPlace(room, occupancy, candidate)) {
            continue;
        }
        auto trial = occupancy;
        Place(room, replacement.instance_id, candidate, trial);
        const bool keeps_dependents = std::ranges::all_of(
            other_cells,
            [&room, &trial](const auto& cells) {
                return std::ranges::all_of(
                    cells,
                    [&room, &trial](const auto& cell) {
                        return cell.tile != FurniturePlacementTile::Support ||
                            SupportSatisfied(room, trial, cell.x, cell.y);
                    });
            });
        if (!keeps_dependents) {
            continue;
        }
        const auto origin_x = candidate.front().x -
            replacement_item.offsets.front().x;
        const auto origin_y = candidate.front().y -
            replacement_item.offsets.front().y;
        const auto delta_x = static_cast<std::int64_t>(origin_x) -
            placed.position_x;
        const auto delta_y = static_cast<std::int64_t>(origin_y) -
            placed.position_y;
        const auto abs_x = delta_x < 0 ? -delta_x : delta_x;
        const auto abs_y = delta_y < 0 ? -delta_y : delta_y;
        const auto rank = std::tuple{
            abs_x + abs_y, abs_y, abs_x, origin_y, origin_x};
        if (!best || rank < best_rank) {
            best = FurnitureReplacementPlacement{
                .room_id = placed.room_id,
                .x = origin_x,
                .y = origin_y};
            best_rank = rank;
        }
    }
    return best;
}

FurnitureLayoutPlan FurnitureLayoutSolver::Plan(
    const std::vector<FurniturePlacement>& furniture,
    const snapshot::detail::HouseGeometryCatalog& geometry,
    const snapshot::detail::FurnitureInfoCatalog& furniture_info,
    const std::vector<FurnitureRoomGrid>& runtime_room_grids,
    const std::vector<snapshot::RoomId>& locked_room_ids) const {
    if (!runtime_room_grids.empty()) {
        return PlanWholeHouse(
            furniture,
            geometry,
            furniture_info,
            runtime_room_grids,
            locked_room_ids);
    }
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
        std::size_t fixed_count{};
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
            item.hitbox_count = static_cast<std::size_t>(
                std::ranges::count_if(item.offsets, [](const auto& cell) {
                    return cell.tile == FurniturePlacementTile::Hitbox;
                }));
            item.solid_count = static_cast<std::size_t>(
                std::ranges::count_if(item.offsets, [](const auto& cell) {
                    return cell.tile == FurniturePlacementTile::Solid;
                }));
            item.surface_count = static_cast<std::size_t>(
                std::ranges::count_if(item.offsets, [](const auto& cell) {
                    return cell.tile == FurniturePlacementTile::Surface;
                }));
            item.support_count = static_cast<std::size_t>(
                std::ranges::count_if(item.offsets, [](const auto& cell) {
                    return cell.tile == FurniturePlacementTile::Support;
                }));
            item.poop_count = static_cast<std::size_t>(
                std::ranges::count_if(item.offsets, [](const auto& cell) {
                    return cell.tile == FurniturePlacementTile::PoopLogic;
                }));
            item.passive = item.hitbox_count == 0U &&
                item.solid_count == 0U && item.support_count == 0U &&
                item.poop_count == 0U;
            if (item.passive) {
                ++passive_count;
                continue;
            }
            item.candidates = GenerateCandidates(room, item);
            if (item.candidates.empty()) {
                room_supported = false;
                break;
            }
            movable.push_back(std::move(item));
        }
        if (!room_supported) {
            plan.unsupported_furniture_count +=
                placements.size() - fixed_count;
            ++plan.current_state_blocked_room_count;
            continue;
        }

        const auto optimizable_count = movable.size() - fixed_count;
        plan.considered_furniture_count += movable.size() + passive_count;
        plan.kept_furniture_count += passive_count;
        if (optimizable_count == 0U) {
            continue;
        }

        std::vector<PackState> packed_candidates;
        const bool preserve_current_stack =
            CurrentAttachmentsAreAlreadyFurnitureAnchored(room, movable);
        if (!preserve_current_stack) {
            for (const auto& order : PackingOrders(movable)) {
                auto packed = PackInOrder(room, movable, order);
                packed_candidates.insert(
                    packed_candidates.end(),
                    std::make_move_iterator(packed.begin()),
                    std::make_move_iterator(packed.end()));
            }
            if (const auto attachments =
                    PackAttachmentsOnCurrentBases(room, movable)) {
                packed_candidates.push_back(*attachments);
            }
        }
        if (const auto current = CurrentPackState(room, movable)) {
            packed_candidates.push_back(*current);
        }
        std::ranges::sort(packed_candidates, BetterPackState);
        const auto unique_end = std::ranges::unique(
            packed_candidates,
            {},
            &PackState::candidate_by_item);
        packed_candidates.erase(unique_end.begin(), unique_end.end());
        if (packed_candidates.empty()) {
            plan.no_space_furniture_count += optimizable_count;
            continue;
        }

        bool current_state_invalid{};
        bool final_state_invalid{};
        bool evacuation_blocked{};
        bool installation_blocked{};
        bool found_complete_plan{};
        for (const auto& candidate : packed_candidates) {
            std::vector<std::vector<MappedCell>> final_cells(
                movable.size());
            bool complete = true;
            for (std::size_t index = 0; index < movable.size(); ++index) {
                if (!candidate.candidate_by_item[index]) {
                    complete = false;
                    break;
                }
                final_cells[index] = movable[index].candidates[
                    *candidate.candidate_by_item[index]];
            }
            if (!complete) {
                final_state_invalid = true;
                continue;
            }

            std::vector<FurnitureLayoutMove> candidate_moves;
            auto kept = plan.kept_furniture_count;
            auto execution = AppendDirectExecutionMoves(
                room,
                room_id,
                movable,
                final_cells,
                candidate_moves,
                kept);
            if (execution != ExecutionPlanResult::Success) {
                candidate_moves.clear();
                kept = plan.kept_furniture_count;
                execution = AppendExecutionMoves(
                    room,
                    room_id,
                    movable,
                    final_cells,
                    candidate_moves,
                    kept);
            }
            if (execution == ExecutionPlanResult::Success) {
                plan.kept_furniture_count = kept;
                found_complete_plan = true;
                if (!candidate_moves.empty()) {
                    plan.moves.insert(
                        plan.moves.end(),
                        std::make_move_iterator(candidate_moves.begin()),
                        std::make_move_iterator(candidate_moves.end()));
                    ++plan.planned_room_count;
                    return plan;
                }
                break;
            }
            current_state_invalid = current_state_invalid ||
                execution == ExecutionPlanResult::CurrentStateInvalid;
            final_state_invalid = final_state_invalid ||
                execution == ExecutionPlanResult::FinalStateInvalid;
            evacuation_blocked = evacuation_blocked ||
                execution == ExecutionPlanResult::EvacuationBlocked;
            installation_blocked = installation_blocked ||
                execution == ExecutionPlanResult::InstallationBlocked;
        }
        if (!found_complete_plan) {
            if (current_state_invalid || final_state_invalid) {
                ++plan.current_state_blocked_room_count;
            }
            if (evacuation_blocked) {
                ++plan.evacuation_blocked_room_count;
            }
            if (installation_blocked) {
                ++plan.installation_blocked_room_count;
            }
            plan.unsupported_furniture_count += optimizable_count;
        }
    }
    return plan;
}

}  // namespace autocattery::furniture_planning
