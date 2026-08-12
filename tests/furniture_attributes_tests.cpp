#include "auto_cattery/snapshot/detail/furniture_attributes.hpp"

#include <cstring>
#include <filesystem>

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

template<class T>
void Append(std::vector<std::byte>& bytes, T value) {
    const auto offset = bytes.size();
    bytes.resize(offset + sizeof(value));
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

snapshot::detail::FurnitureStorageRecord FurnitureRecord(
    std::string_view item,
    std::string_view room,
    std::int64_t key = 17,
    std::uint32_t version = 1,
    std::uint64_t placement_flags = 2,
    std::int32_t scale_x = 1,
    std::int32_t scale_y = 1) {
    snapshot::detail::FurnitureStorageRecord record;
    record.key = key;
    Append<std::uint32_t>(record.blob, version);
    Append<std::uint32_t>(
        record.blob, static_cast<std::uint32_t>(item.size()));
    Append<std::uint32_t>(record.blob, 7);
    for (const char byte : item) {
        record.blob.push_back(static_cast<std::byte>(byte));
    }
    Append<std::uint64_t>(record.blob, placement_flags);
    Append<std::uint32_t>(
        record.blob, static_cast<std::uint32_t>(room.size()));
    Append<std::uint32_t>(record.blob, 9);
    for (const char byte : room) {
        record.blob.push_back(static_cast<std::byte>(byte));
    }
    Append<std::int32_t>(record.blob, -3);
    Append<std::int32_t>(record.blob, -7);
    Append<std::uint32_t>(record.blob, 11);
    Append<std::int32_t>(record.blob, scale_x);
    Append<std::int32_t>(record.blob, scale_y);
    return record;
}

}  // namespace

void RunFurnitureAttributesTests() {
    std::vector<snapshot::detail::FurniturePlacement> placements;
    std::string error;
    AC_CHECK(snapshot::detail::ParseFurniturePlacements(
        {FurnitureRecord("chair", "Attic")}, placements, error));
    AC_CHECK(placements.size() == 1);
    AC_CHECK(placements[0].instance_id == 17);
    AC_CHECK(placements[0].format_version == 1);
    AC_CHECK(placements[0].item_id == "chair");
    AC_CHECK(placements[0].unknown_after_item_length == 7);
    AC_CHECK(placements[0].placement_flags == 2);
    AC_CHECK(placements[0].HasOnlyKnownPlacementFlags());
    AC_CHECK(placements[0].IsRare());
    AC_CHECK(placements[0].room_id == "Attic");
    AC_CHECK(placements[0].unknown_after_room_length == 9);
    AC_CHECK(placements[0].position_x == -3);
    AC_CHECK(placements[0].position_y == -7);
    AC_CHECK(placements[0].position_z == 11);
    AC_CHECK(placements[0].scale_x == 1);
    AC_CHECK(placements[0].scale_y == 1);
    AC_CHECK(placements[0].HasSupportedGridScale());
    const auto normal_cell = placements[0].MapGridCellToRoom(5, 3);
    AC_CHECK(normal_cell.has_value());
    AC_CHECK(normal_cell->x == 2);
    AC_CHECK(normal_cell->y == -4);

    placements.clear();
    AC_CHECK(snapshot::detail::ParseFurniturePlacements(
        {FurnitureRecord("lamp", "Attic", 23, 1, 6)}, placements, error));
    AC_CHECK(placements.size() == 1);
    AC_CHECK(placements[0].placement_flags == 6);
    AC_CHECK(!placements[0].HasOnlyKnownPlacementFlags());
    AC_CHECK(placements[0].IsRare());

    placements.clear();
    AC_CHECK(snapshot::detail::ParseFurniturePlacements(
        {FurnitureRecord("mirror", "Attic", 24, 1, 0, -1, 1)},
        placements, error));
    AC_CHECK(placements[0].scale_x == -1);
    AC_CHECK(placements[0].scale_y == 1);
    const auto flipped_cell = placements[0].MapGridCellToRoom(5, 3);
    AC_CHECK(flipped_cell.has_value());
    AC_CHECK(flipped_cell->x == -8);
    AC_CHECK(flipped_cell->y == -4);

    placements.clear();
    AC_CHECK(snapshot::detail::ParseFurniturePlacements(
        {FurnitureRecord("vertical", "Attic", 25, 1, 0, 1, -1)},
        placements, error));
    const auto vertical_cell = placements[0].MapGridCellToRoom(5, 3);
    AC_CHECK(vertical_cell.has_value());
    AC_CHECK(vertical_cell->x == 2);
    AC_CHECK(vertical_cell->y == -10);

    placements.clear();
    AC_CHECK(snapshot::detail::ParseFurniturePlacements(
        {FurnitureRecord("unsupported", "Attic", 26, 1, 0, 0, 1)},
        placements, error));
    AC_CHECK(!placements[0].HasSupportedGridScale());
    AC_CHECK(!placements[0].MapGridCellToRoom(1, 1).has_value());

    placements.clear();
    AC_CHECK(snapshot::detail::ParseFurniturePlacements(
        {FurnitureRecord("table", "", 18)}, placements, error));
    AC_CHECK(placements.size() == 1);
    AC_CHECK(placements[0].room_id.empty());

    auto truncated = FurnitureRecord("table", "Attic", 19);
    truncated.blob.pop_back();
    AC_CHECK(!snapshot::detail::ParseFurniturePlacements(
        {truncated}, placements, error));

    auto trailing = FurnitureRecord("table", "Attic", 20);
    trailing.blob.push_back(std::byte{0});
    AC_CHECK(!snapshot::detail::ParseFurniturePlacements(
        {trailing}, placements, error));

    AC_CHECK(!snapshot::detail::ParseFurniturePlacements(
        {FurnitureRecord("table", "Attic", 21, 2)}, placements, error));

    AC_CHECK(!snapshot::detail::ParseFurniturePlacements(
        {FurnitureRecord("chair", "Attic", 22),
         FurnitureRecord("table", "Attic", 22)}, placements, error));

    snapshot::HouseSnapshot house;
    house.rooms.push_back({.id = "Attic"});
    snapshot::detail::FurnitureCatalog catalog;
    catalog["chair"] = {
        .comfort = 3,
        .stimulation = 2,
        .health = 1,
        .mutation = 4
    };
    snapshot::detail::ApplyFurnitureRoomAttributes(
        house, placements, catalog);
    AC_CHECK(house.capabilities.read_room_attributes);
    AC_CHECK(house.rooms[0].attributes->comfort == 3);
    AC_CHECK(house.rooms[0].attributes->stimulation == 2);

    const auto game_root = std::filesystem::path(__FILE__)
        .parent_path().parent_path().parent_path();
    if (std::filesystem::exists(game_root / "resources.gpak")) {
        snapshot::detail::FurnitureCatalog live;
        AC_CHECK(snapshot::detail::LoadFurnitureCatalog(
            game_root / "resources.gpak", live, error));
        AC_CHECK(live.size() > 600);
        const auto& comfort = live.at("special_comfortidol");
        AC_CHECK(comfort.comfort == 5);
        AC_CHECK(comfort.stimulation == 0);
        AC_CHECK(comfort.health == 0);
        AC_CHECK(comfort.mutation == 0);
        AC_CHECK(comfort.appeal == 0);

        const auto& stimulation = live.at("special_stimulationidol");
        AC_CHECK(stimulation.comfort == 0);
        AC_CHECK(stimulation.stimulation == 5);
        AC_CHECK(stimulation.health == 0);
        AC_CHECK(stimulation.mutation == 0);
        AC_CHECK(stimulation.appeal == 0);

        const auto& health = live.at("special_healthidol");
        AC_CHECK(health.comfort == 0);
        AC_CHECK(health.stimulation == 0);
        AC_CHECK(health.health == 5);
        AC_CHECK(health.mutation == 0);
        AC_CHECK(health.appeal == 0);

        const auto& evolution = live.at("special_evolutionidol");
        AC_CHECK(evolution.comfort == 0);
        AC_CHECK(evolution.stimulation == 0);
        AC_CHECK(evolution.health == 0);
        AC_CHECK(evolution.mutation == 5);
        AC_CHECK(evolution.appeal == 0);

        const auto& appeal = live.at("special_appealidol");
        AC_CHECK(appeal.comfort == 0);
        AC_CHECK(appeal.stimulation == 0);
        AC_CHECK(appeal.health == 0);
        AC_CHECK(appeal.mutation == 0);
        AC_CHECK(appeal.appeal == 5);

        AC_CHECK(live.at("special_fightidol").comfort == -5);
    }
}

}  // namespace autocattery::tests
