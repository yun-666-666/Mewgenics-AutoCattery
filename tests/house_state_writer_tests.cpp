#include "auto_cattery/snapshot/house_state_writer.hpp"

#include <limits>

#include "test_support.hpp"

namespace autocattery::tests {

void RunHouseStateWriterTests() {
    const std::vector<snapshot::HouseStateEntry> entries{
        {11, "Floor1_Large", 1.0, 2.0, 3.0},
        {12, "Attic", 4.0, 5.0, 6.0},
        {13, "AdventureBox", 0.5, 1.9, 0.0}
    };
    const auto encoded = snapshot::SerializeHouseState(entries);
    AC_CHECK(static_cast<bool>(encoded));
    const auto parsed = snapshot::ParseHouseState(encoded.value);
    AC_CHECK(static_cast<bool>(parsed));
    AC_CHECK(parsed.value.size() == entries.size());
    AC_CHECK(parsed.value[0].room_id == "Floor1_Large");

    const auto moved = snapshot::BuildSingleCatTestRelocation(
        encoded.value, 0, 1);
    AC_CHECK(static_cast<bool>(moved));
    AC_CHECK(moved.value.original.room_id == "Floor1_Large");
    AC_CHECK(moved.value.relocated.room_id == "Attic");
    const auto moved_readback = snapshot::ParseHouseState(
        moved.value.encoded_house_state);
    AC_CHECK(static_cast<bool>(moved_readback));
    AC_CHECK(moved_readback.value[0].room_id == "Attic");
    AC_CHECK(moved_readback.value[0].position_x == 4.0);
    AC_CHECK(moved_readback.value[1].room_id == "Attic");

    AC_CHECK(!snapshot::BuildSingleCatTestRelocation(encoded.value, 0, 0));
    AC_CHECK(!snapshot::BuildSingleCatTestRelocation(encoded.value, 0, 3));
    AC_CHECK(!snapshot::BuildSingleCatTestRelocation(encoded.value, 0, 2));
    auto unknown_room_entries = entries;
    unknown_room_entries[1].room_id = "UnverifiedRoom";
    const auto unknown_room = snapshot::SerializeHouseState(unknown_room_entries);
    AC_CHECK(static_cast<bool>(unknown_room));
    AC_CHECK(!snapshot::BuildSingleCatTestRelocation(
        unknown_room.value, 0, 1));

    auto duplicate = entries;
    duplicate[1].cat_id = duplicate[0].cat_id;
    AC_CHECK(!snapshot::SerializeHouseState(duplicate));
    auto invalid_position = entries;
    invalid_position[0].position_x =
        std::numeric_limits<double>::quiet_NaN();
    AC_CHECK(!snapshot::SerializeHouseState(invalid_position));
    auto invalid_room = entries;
    invalid_room[0].room_id = std::string(129, 'x');
    AC_CHECK(!snapshot::SerializeHouseState(invalid_room));
}

}  // namespace autocattery::tests
