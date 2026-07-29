#include "auto_cattery/snapshot/detail/snapshot_assembler.hpp"

#include "test_support.hpp"

#include <string>
#include <vector>

namespace autocattery::tests {

void RunSnapshotAssemblerTests() {
    std::vector<snapshot::CatSnapshot> cats{
        {.id = 11, .display_name = "one"},
        {.id = 12, .display_name = "two"},
        {.id = 13, .display_name = "three"}
    };
    const std::vector<snapshot::HouseStateEntry> entries{
        {.cat_id = 11, .room_id = "Floor1_Large"},
        {.cat_id = 12, .room_id = "AdventureBox"}
    };

    const auto assembled = snapshot::detail::AssembleHouseSnapshot(
        5, 7, 17, "campaign.sav", cats, entries);
    AC_CHECK(static_cast<bool>(assembled));
    AC_CHECK(assembled.value.snapshot_id == 5);
    AC_CHECK(assembled.value.scene_generation == 7);
    AC_CHECK(assembled.value.game_day == 17);
    AC_CHECK(assembled.value.cats.size() == 2);
    AC_CHECK(assembled.value.rooms.size() == 2);
    AC_CHECK(assembled.value.cats[0].room_id == "Floor1_Large");
    AC_CHECK(assembled.value.cats[1].in_adventure_box);
    AC_CHECK(assembled.value.capabilities.read_room_assignments);
    AC_CHECK(!assembled.value.capabilities.read_room_capacities);
    AC_CHECK(!assembled.value.capabilities.read_class_id);
    AC_CHECK(!assembled.value.capabilities.read_age);
    AC_CHECK(snapshot::Validate(assembled.value).Valid());

    auto duplicate_entries = entries;
    duplicate_entries.push_back(
        {.cat_id = 11, .room_id = "Floor1_Small"});
    AC_CHECK(!static_cast<bool>(
        snapshot::detail::AssembleHouseSnapshot(
            6, 7, 17, "campaign.sav", cats, duplicate_entries)));

    const std::vector<snapshot::HouseStateEntry> missing_cat{
        {.cat_id = 99, .room_id = "Attic"}
    };
    AC_CHECK(!static_cast<bool>(
        snapshot::detail::AssembleHouseSnapshot(
            7, 7, 17, "campaign.sav", cats, missing_cat)));

    const auto empty = snapshot::detail::AssembleHouseSnapshot(
        8, 7, 17, "campaign.sav", {}, {});
    AC_CHECK(static_cast<bool>(empty));
    AC_CHECK(empty.value.cats.empty());
    AC_CHECK(empty.value.rooms.empty());

    for (const std::size_t count : {100U, 500U, 1000U}) {
        std::vector<snapshot::CatSnapshot> many_cats;
        std::vector<snapshot::HouseStateEntry> many_entries;
        many_cats.reserve(count);
        many_entries.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            const auto id = static_cast<snapshot::CatId>(index + 1);
            many_cats.push_back({.id = id});
            many_entries.push_back({
                .cat_id = id,
                .room_id = "Room" + std::to_string(index % 10)
            });
        }
        const auto large = snapshot::detail::AssembleHouseSnapshot(
            9, 7, 17, "campaign.sav",
            std::move(many_cats), many_entries);
        AC_CHECK(static_cast<bool>(large));
        AC_CHECK(large.value.cats.size() == count);
        AC_CHECK(large.value.rooms.size() == 10);
    }
}

}  // namespace autocattery::tests
