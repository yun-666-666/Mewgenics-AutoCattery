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
    std::uint32_t version = 1) {
    snapshot::detail::FurnitureStorageRecord record;
    record.key = key;
    Append<std::uint32_t>(record.blob, version);
    Append<std::uint32_t>(
        record.blob, static_cast<std::uint32_t>(item.size()));
    Append<std::uint32_t>(record.blob, 7);
    for (const char byte : item) {
        record.blob.push_back(static_cast<std::byte>(byte));
    }
    Append<std::uint64_t>(record.blob, 42);
    Append<std::uint32_t>(
        record.blob, static_cast<std::uint32_t>(room.size()));
    Append<std::uint32_t>(record.blob, 9);
    for (const char byte : room) {
        record.blob.push_back(static_cast<std::byte>(byte));
    }
    Append<std::int32_t>(record.blob, -3);
    Append<std::int32_t>(record.blob, -7);
    Append<std::uint32_t>(record.blob, 11);
    Append<std::uint32_t>(record.blob, 1);
    Append<std::uint32_t>(record.blob, 1);
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
    AC_CHECK(placements[0].unknown_before_room == 42);
    AC_CHECK(placements[0].room_id == "Attic");
    AC_CHECK(placements[0].unknown_after_room_length == 9);
    AC_CHECK(placements[0].position_x == -3);
    AC_CHECK(placements[0].position_y == -7);
    AC_CHECK(placements[0].position_z == 11);
    AC_CHECK(placements[0].unknown_flag_1 == 1);
    AC_CHECK(placements[0].unknown_flag_2 == 1);

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
        AC_CHECK(live.at("special_comfortidol").comfort == 5);
    }
}

}  // namespace autocattery::tests
