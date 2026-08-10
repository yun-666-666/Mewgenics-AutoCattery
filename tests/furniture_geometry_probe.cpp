#include "auto_cattery/snapshot/detail/furniture_attributes.hpp"
#include "auto_cattery/snapshot/detail/furniture_geometry.hpp"
#include "auto_cattery/snapshot/detail/save_database.hpp"
#include "auto_cattery/snapshot/detail/save_locator.hpp"
#include "auto_cattery/furniture_planning/layout_solver.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

int wmain(int argument_count, wchar_t** arguments) {
    if (argument_count < 2 || argument_count > 4) {
        std::cerr << "usage: furniture_geometry_probe <game-root> [save] [room=widthxheight;...]\n";
        return 2;
    }
    const std::filesystem::path game_root(arguments[1]);
    const std::filesystem::path configured_save =
        argument_count == 3 ? std::filesystem::path(arguments[2]) :
        argument_count == 4 ? std::filesystem::path(arguments[2]) :
                              std::filesystem::path{};
    std::vector<autocattery::furniture_planning::FurnitureRoomGrid>
        runtime_room_grids;
    if (argument_count == 4) {
        const std::wstring specification(arguments[3]);
        std::size_t begin{};
        while (begin < specification.size()) {
            const auto end = specification.find(L';', begin);
            const auto token = specification.substr(
                begin,
                end == std::wstring::npos
                    ? std::wstring::npos
                    : end - begin);
            const auto equals = token.find(L'=');
            const auto times = token.find(L'x', equals + 1U);
            if (equals == std::wstring::npos ||
                times == std::wstring::npos) {
                std::cerr << "invalid runtime room grid specification\n";
                return 2;
            }
            const auto room = token.substr(0U, equals);
            std::string room_id;
            room_id.reserve(room.size());
            for (const auto character : room) {
                if (character > 0x7f) {
                    std::cerr << "runtime room id must be ASCII\n";
                    return 2;
                }
                room_id.push_back(static_cast<char>(character));
            }
            runtime_room_grids.push_back({
                std::move(room_id),
                static_cast<std::size_t>(std::stoull(
                    token.substr(equals + 1U, times - equals - 1U))),
                static_cast<std::size_t>(std::stoull(
                    token.substr(times + 1U)))});
            if (end == std::wstring::npos) {
                break;
            }
            begin = end + 1U;
        }
    }
    std::string error;
    const auto save = autocattery::snapshot::detail::FindMostRecentSave(
        configured_save, error);
    if (!save) {
        std::cerr << "save discovery failed: " << error << '\n';
        return 1;
    }
    auto database = autocattery::snapshot::detail::SaveDatabase::OpenReadOnly(
        *save, error);
    if (!database) {
        std::cerr << "read-only save open failed: " << error << '\n';
        return 1;
    }

    std::optional<std::int32_t> day;
    std::vector<autocattery::snapshot::detail::FurnitureStorageRecord> stored;
    if (!database->ReadCurrentDay(day, error) ||
        !database->ReadFurniture(stored, error)) {
        std::cerr << "read-only furniture query failed: " << error << '\n';
        return 1;
    }
    std::vector<autocattery::snapshot::detail::FurniturePlacement> placements;
    if (!autocattery::snapshot::detail::ParseFurniturePlacements(
            stored, placements, error)) {
        std::cerr << "furniture parse failed: " << error << '\n';
        return 1;
    }

    autocattery::snapshot::detail::HouseGeometryCatalog geometry;
    autocattery::snapshot::detail::FurnitureInfoCatalog furniture_info;
    autocattery::snapshot::detail::FurnitureCatalog effects;
    const auto resources = game_root / L"resources.gpak";
    if (!autocattery::snapshot::detail::LoadHouseGeometryCatalog(
            resources, geometry, error) ||
        !autocattery::snapshot::detail::LoadFurnitureInfoCatalog(
            resources, furniture_info, error) ||
        !autocattery::snapshot::detail::LoadFurnitureCatalog(
            resources, effects, error)) {
        std::cerr << "resource parse failed: " << error << '\n';
        return 1;
    }

    std::map<
        std::string,
        const autocattery::snapshot::detail::FurnitureInfoRecord*> info_by_id;
    for (const auto& record : furniture_info.records) {
        info_by_id.emplace(record.item_id, &record);
    }
    std::size_t placed{};
    std::size_t warehouse{};
    std::size_t info_coverage{};
    std::size_t placement_grid_coverage{};
    std::size_t effect_coverage{};
    std::map<
        autocattery::snapshot::detail::FurniturePlacementTile,
        std::size_t> active_tile_counts;
    std::set<std::string> observed_rooms;
    std::map<std::string, std::size_t> room_counts;
    std::map<std::uint64_t, std::size_t> placement_flag_values;
    std::size_t supported_placement_flag_records{};
    std::size_t rare_records{};
    std::map<std::uint32_t, std::size_t> item_length_unknown_values;
    std::map<std::uint32_t, std::size_t> room_length_unknown_values;
    std::map<std::uint32_t, std::size_t> info_name_unknown_values;
    std::map<std::pair<std::int32_t, std::int32_t>, std::size_t> scale_pairs;
    std::size_t supported_scale_records{};
    std::size_t horizontal_flip_records{};
    std::size_t vertical_flip_records{};
    std::int32_t minimum_x = std::numeric_limits<std::int32_t>::max();
    std::int32_t maximum_x = std::numeric_limits<std::int32_t>::min();
    std::int32_t minimum_y = std::numeric_limits<std::int32_t>::max();
    std::int32_t maximum_y = std::numeric_limits<std::int32_t>::min();
    std::uint32_t minimum_z = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t maximum_z = std::numeric_limits<std::uint32_t>::min();
    for (const auto& placement : placements) {
        if (placement.room_id.empty()) {
            ++warehouse;
        } else {
            ++placed;
            observed_rooms.insert(placement.room_id);
            ++room_counts[placement.room_id];
        }
        const auto info = info_by_id.find(placement.item_id);
        if (info != info_by_id.end()) {
            ++info_coverage;
            if (info->second->placement_grid.supported) {
                ++placement_grid_coverage;
                for (const auto tile : info->second->placement_grid.tiles) {
                    ++active_tile_counts[tile];
                }
            }
        }
        effect_coverage += effects.contains(placement.item_id) ? 1U : 0U;
        ++placement_flag_values[placement.placement_flags];
        supported_placement_flag_records +=
            placement.HasOnlyKnownPlacementFlags() ? 1U : 0U;
        rare_records += placement.IsRare() ? 1U : 0U;
        ++item_length_unknown_values[
            placement.unknown_after_item_length];
        ++room_length_unknown_values[
            placement.unknown_after_room_length];
        ++scale_pairs[{placement.scale_x, placement.scale_y}];
        const bool supported_scale = placement.HasSupportedGridScale();
        supported_scale_records += supported_scale ? 1U : 0U;
        horizontal_flip_records +=
            supported_scale && placement.scale_x == -1 ? 1U : 0U;
        vertical_flip_records +=
            supported_scale && placement.scale_y == -1 ? 1U : 0U;
        minimum_x = std::min(minimum_x, placement.position_x);
        maximum_x = std::max(maximum_x, placement.position_x);
        minimum_y = std::min(minimum_y, placement.position_y);
        maximum_y = std::max(maximum_y, placement.position_y);
        minimum_z = std::min(minimum_z, placement.position_z);
        maximum_z = std::max(maximum_z, placement.position_z);
    }

    std::cout
        << "mode=read_only"
        << " day=" << (day ? std::to_string(*day) : "unavailable")
        << " furniture=" << placements.size()
        << " placed=" << placed
        << " warehouse=" << warehouse
        << " observed_rooms=" << observed_rooms.size()
        << " info_coverage=" << info_coverage << '/' << placements.size()
        << " placement_grid_coverage=" << placement_grid_coverage << '/'
        << placements.size()
        << " effect_coverage=" << effect_coverage << '/' << placements.size()
        << '\n';
    if (!placements.empty()) {
        std::cout
            << "coordinates x=" << minimum_x << ".." << maximum_x
            << " y=" << minimum_y << ".." << maximum_y
            << " z=" << minimum_z << ".." << maximum_z << '\n';
    }
    for (const auto& [room, count] : room_counts) {
        std::cout << "observed_room=" << room
                  << " furniture=" << count << '\n';
    }
    std::cout
        << "placement_flags_supported="
        << supported_placement_flag_records << '/' << placements.size()
        << " rare=" << rare_records << '\n';
    for (const auto& [value, count] : placement_flag_values) {
        std::cout << "placement_flags_raw=" << value
                  << " count=" << count << '\n';
    }
    for (const auto& [value, count] : item_length_unknown_values) {
        std::cout << "unknown_after_item_length=" << value
                  << " count=" << count << '\n';
    }
    for (const auto& [value, count] : room_length_unknown_values) {
        std::cout << "unknown_after_room_length=" << value
                  << " count=" << count << '\n';
    }
    std::cout
        << "placement_scales_supported="
        << supported_scale_records << '/' << placements.size()
        << " horizontal_flipped=" << horizontal_flip_records
        << " vertical_flipped=" << vertical_flip_records << '\n';
    for (const auto& [scales, count] : scale_pairs) {
        std::cout << "placement_scales=" << scales.first << ',' << scales.second
                  << " count=" << count << '\n';
    }
    std::cout
        << "resource_rooms=" << geometry.rooms.size()
        << " resource_houses=" << geometry.houses.size()
        << " furniture_info_version=" << furniture_info.format_version
        << " furniture_info_records=" << furniture_info.records.size()
        << '\n';
    std::size_t supported_grid_records{};
    std::size_t records_with_nonzero_outside_grid{};
    for (const auto& record : furniture_info.records) {
        ++info_name_unknown_values[record.unknown_after_name_length];
        supported_grid_records += record.placement_grid.supported ? 1U : 0U;
        records_with_nonzero_outside_grid +=
            record.nonzero_bytes_outside_placement_grid != 0U ? 1U : 0U;
    }
    std::cout
        << "placement_grid="
        << autocattery::snapshot::detail::kFurniturePlacementGridWidth
        << 'x'
        << autocattery::snapshot::detail::kFurniturePlacementGridHeight
        << " supported_records=" << supported_grid_records << '/'
        << furniture_info.records.size()
        << " nonzero_outside_grid_records="
        << records_with_nonzero_outside_grid
        << '\n';
    std::cout
        << "active_tiles"
        << " hitbox=" << active_tile_counts[
            autocattery::snapshot::detail::FurniturePlacementTile::Hitbox]
        << " solid=" << active_tile_counts[
            autocattery::snapshot::detail::FurniturePlacementTile::Solid]
        << " support=" << active_tile_counts[
            autocattery::snapshot::detail::FurniturePlacementTile::Support]
        << " surface=" << active_tile_counts[
            autocattery::snapshot::detail::FurniturePlacementTile::Surface]
        << " poop=" << active_tile_counts[
            autocattery::snapshot::detail::FurniturePlacementTile::PoopLogic]
        << '\n';
    for (const auto& [value, count] : info_name_unknown_values) {
        std::cout << "furniture_info_unknown_after_name_length=" << value
                  << " count=" << count << '\n';
    }
    const auto layout =
        autocattery::furniture_planning::FurnitureLayoutSolver{}.Plan(
            placements, geometry, furniture_info, runtime_room_grids);
    std::cout
        << "layout_rooms=" << layout.planned_room_count
        << " considered=" << layout.considered_furniture_count
        << " moves=" << layout.moves.size()
        << " kept=" << layout.kept_furniture_count
        << " deferred=" << layout.deferred_furniture_count
        << " target=" << layout.target_room_id
        << " warehouse=" << layout.warehouse_furniture_count
        << " unsupported=" << layout.unsupported_furniture_count
        << " no_space=" << layout.no_space_furniture_count
        << " current_blocked="
        << layout.current_state_blocked_room_count
        << " evacuation_blocked="
        << layout.evacuation_blocked_room_count
        << " installation_blocked="
        << layout.installation_blocked_room_count
        << '\n';
    for (const auto& placement : placements) {
        const auto info = info_by_id.find(placement.item_id);
        if (info == info_by_id.end()) {
            continue;
        }
        std::map<
            autocattery::snapshot::detail::FurniturePlacementTile,
            std::size_t> counts;
        for (const auto tile : info->second->placement_grid.tiles) {
            if (tile !=
                autocattery::snapshot::detail::FurniturePlacementTile::Empty) {
                ++counts[tile];
            }
        }
        std::cout
            << "layout_shape item=" << placement.item_id
            << " key=" << placement.instance_id
            << " pos=" << placement.position_x << ','
            << placement.position_y
            << " hitbox=" << counts[
                autocattery::snapshot::detail::FurniturePlacementTile::Hitbox]
            << " solid=" << counts[
                autocattery::snapshot::detail::FurniturePlacementTile::Solid]
            << " support=" << counts[
                autocattery::snapshot::detail::FurniturePlacementTile::Support]
            << " surface=" << counts[
                autocattery::snapshot::detail::FurniturePlacementTile::Surface]
            << " poop=" << counts[
                autocattery::snapshot::detail::FurniturePlacementTile::PoopLogic]
            << '\n';
        const auto room_definition = std::find_if(
            geometry.rooms.begin(),
            geometry.rooms.end(),
            [&placement](const auto& room) {
                return room.room_id == placement.room_id;
            });
        if (room_definition == geometry.rooms.end()) {
            continue;
        }
        const auto room_grid =
            autocattery::snapshot::detail::DecodeRoomCollisionGrid(
                *room_definition);
        std::map<std::pair<int, int>, std::size_t> relations;
        std::size_t outside{};
        for (std::size_t y = 0;
             y < autocattery::snapshot::detail::kFurniturePlacementGridHeight;
             ++y) {
            for (std::size_t x = 0;
                 x < autocattery::snapshot::detail::kFurniturePlacementGridWidth;
                 ++x) {
                const auto tile = info->second->placement_grid.At(x, y);
                if (tile ==
                    autocattery::snapshot::detail::FurniturePlacementTile::Empty) {
                    continue;
                }
                const auto mapped = placement.MapGridCellToRoom(
                    static_cast<std::int32_t>(x),
                    static_cast<std::int32_t>(y));
                if (!mapped || mapped->x < 0 || mapped->y < 0 ||
                    static_cast<std::size_t>(mapped->x) >= room_grid.width ||
                    static_cast<std::size_t>(mapped->y) >= room_grid.height) {
                    ++outside;
                    continue;
                }
                ++relations[{
                    static_cast<int>(tile),
                    room_grid.At(
                        static_cast<std::size_t>(mapped->x),
                        static_cast<std::size_t>(mapped->y))}];
            }
        }
        std::cout << "layout_relation item=" << placement.item_id
                  << " key=" << placement.instance_id
                  << " outside=" << outside;
        for (const auto& [relation, count] : relations) {
            std::cout << " t" << relation.first << "r" << relation.second
                      << '=' << count;
        }
        std::cout << '\n';
    }
    for (const auto& move : layout.moves) {
        std::cout
            << "layout_move item=" << move.item_id
            << " key=" << move.stable_key
            << " room=" << move.from_room_id << "->"
            << move.target_room_id
            << " from=" << move.from_x << ',' << move.from_y
            << " target=" << move.target_x << ',' << move.target_y
            << '\n';
    }
    for (const auto& room : geometry.rooms) {
        const auto collision_rows = room.built_in_collision.size();
        const auto collision_columns = collision_rows == 0U ? 0U :
            room.built_in_collision.front().size();
        const auto runtime_collision =
            autocattery::snapshot::detail::DecodeRoomCollisionGrid(room);
        std::cout
            << "room_definition=" << room.definition_id
            << " room_id=" << room.room_id
            << " size=" << room.width << 'x' << room.height
            << " collision=" << collision_columns << 'x' << collision_rows
            << " runtime_collision=" << runtime_collision.width << 'x'
            << runtime_collision.height
            << " supported=" << (runtime_collision.supported ? 1 : 0)
            << '\n';
    }
    for (const auto& house : geometry.houses) {
        std::cout
            << "house_layout=" << house.house_id
            << " room_positions=" << house.room_positions.size()
            << '\n';
    }
    return 0;
}
