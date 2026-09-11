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
    std::string_view room) {
    snapshot::detail::FurnitureStorageRecord record;
    Append<std::uint32_t>(record.blob, 1);
    Append<std::uint64_t>(record.blob, item.size());
    for (const char byte : item) {
        record.blob.push_back(static_cast<std::byte>(byte));
    }
    Append<std::uint32_t>(record.blob, 0);
    Append<std::uint32_t>(record.blob, 0);
    Append<std::uint32_t>(
        record.blob, static_cast<std::uint32_t>(room.size()));
    Append<std::uint32_t>(record.blob, 0);
    for (const char byte : room) {
        record.blob.push_back(static_cast<std::byte>(byte));
    }
    return record;
}

}  // namespace

void RunFurnitureAttributesTests() {
    std::vector<snapshot::detail::FurniturePlacement> placements;
    std::string error;
    AC_CHECK(snapshot::detail::ParseFurniturePlacements(
        {FurnitureRecord("chair", "Attic")}, placements, error));
    AC_CHECK(placements.size() == 1);
    AC_CHECK(placements[0].item_id == "chair");
    AC_CHECK(placements[0].room_id == "Attic");

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

    // Occupancy and poop cannot change the furnished baseline, and a room
    // with furniture but no cats must not disappear from purpose selection.
    house.rooms[0].residents.resize(100);
    placements.push_back({"poop", "Attic"});
    placements.push_back({"chair", "Floor1_Large"});
    catalog["poop"] = {.comfort = -100};
    snapshot::detail::ApplyFurnitureRoomAttributes(house, placements, catalog);
    AC_CHECK(house.rooms.size() == 2);
    AC_CHECK(house.rooms[0].attributes->comfort == 3);
    AC_CHECK(house.rooms[1].id == "Floor1_Large");
    AC_CHECK(house.rooms[1].residents.empty());
    AC_CHECK(house.rooms[1].attributes->comfort == 3);

    std::vector<std::byte> unlocks;
    Append<std::uint32_t>(unlocks, 1);
    const auto append_string = [&](std::string_view text) {
        Append<std::uint64_t>(unlocks, text.size());
        for (const auto ch : text) unlocks.push_back(static_cast<std::byte>(ch));
    };
    append_string("House3");
    Append<std::uint64_t>(unlocks, 5);
    for (const auto upgrade : {"Default", "SmallHouse_Attic", "MediumHouse_SmallRoom",
                              "LargeHouse_Floor2Large", "LargeHouse_Floor2Small"}) {
        append_string(upgrade);
    }
    snapshot::HouseSnapshot empty_house;
    AC_CHECK(snapshot::detail::ApplyUnlockedHouseRooms(empty_house, unlocks));
    AC_CHECK(empty_house.rooms.size() == 5);
    AC_CHECK(empty_house.rooms.back().id == "Floor2_Small");
    AC_CHECK(snapshot::detail::ApplyUnlockedHouseRooms(empty_house, unlocks));
    AC_CHECK(empty_house.rooms.size() == 5);
    auto truncated = unlocks;
    truncated.pop_back();
    snapshot::HouseSnapshot incomplete;
    AC_CHECK(!snapshot::detail::ApplyUnlockedHouseRooms(incomplete, truncated));
    AC_CHECK(incomplete.rooms.empty());

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
