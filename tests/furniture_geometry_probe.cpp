#include "auto_cattery/snapshot/detail/furniture_attributes.hpp"
#include "auto_cattery/snapshot/detail/furniture_geometry.hpp"
#include "auto_cattery/snapshot/detail/save_database.hpp"
#include "auto_cattery/snapshot/detail/save_locator.hpp"

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
    if (argument_count < 2 || argument_count > 3) {
        std::cerr << "usage: furniture_geometry_probe <game-root> [save]\n";
        return 2;
    }
    const std::filesystem::path game_root(arguments[1]);
    const std::filesystem::path configured_save =
        argument_count == 3 ? std::filesystem::path(arguments[2]) :
                              std::filesystem::path{};
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

    std::unordered_set<std::string> info_ids;
    for (const auto& record : furniture_info.records) {
        info_ids.insert(record.item_id);
    }
    std::size_t placed{};
    std::size_t warehouse{};
    std::size_t info_coverage{};
    std::size_t effect_coverage{};
    std::set<std::string> observed_rooms;
    std::map<std::string, std::size_t> room_counts;
    std::map<std::uint64_t, std::size_t> unknown_values;
    std::map<std::uint32_t, std::size_t> item_length_unknown_values;
    std::map<std::uint32_t, std::size_t> room_length_unknown_values;
    std::map<std::uint32_t, std::size_t> info_name_unknown_values;
    std::map<std::pair<std::uint32_t, std::uint32_t>, std::size_t> flag_pairs;
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
        info_coverage += info_ids.contains(placement.item_id) ? 1U : 0U;
        effect_coverage += effects.contains(placement.item_id) ? 1U : 0U;
        ++unknown_values[placement.unknown_before_room];
        ++item_length_unknown_values[
            placement.unknown_after_item_length];
        ++room_length_unknown_values[
            placement.unknown_after_room_length];
        ++flag_pairs[{
            placement.unknown_flag_1, placement.unknown_flag_2}];
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
    for (const auto& [value, count] : unknown_values) {
        std::cout << "unknown_before_room=" << value
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
    for (const auto& [flags, count] : flag_pairs) {
        std::cout << "unknown_flags=" << flags.first << ',' << flags.second
                  << " count=" << count << '\n';
    }
    std::cout
        << "resource_rooms=" << geometry.rooms.size()
        << " resource_houses=" << geometry.houses.size()
        << " furniture_info_version=" << furniture_info.format_version
        << " furniture_info_records=" << furniture_info.records.size()
        << '\n';
    for (const auto& record : furniture_info.records) {
        ++info_name_unknown_values[record.unknown_after_name_length];
    }
    for (const auto& [value, count] : info_name_unknown_values) {
        std::cout << "furniture_info_unknown_after_name_length=" << value
                  << " count=" << count << '\n';
    }
    for (const auto& room : geometry.rooms) {
        const auto collision_rows = room.built_in_collision.size();
        const auto collision_columns = collision_rows == 0U ? 0U :
            room.built_in_collision.front().size();
        std::cout
            << "room_definition=" << room.definition_id
            << " room_id=" << room.room_id
            << " size=" << room.width << 'x' << room.height
            << " collision=" << collision_columns << 'x' << collision_rows
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
