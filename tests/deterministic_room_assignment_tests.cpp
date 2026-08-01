#include "auto_cattery/workflow/preview_builder.hpp"

#include <algorithm>
#include <unordered_map>

#include "test_support.hpp"
#include "workflow_test_fixture.hpp"

namespace autocattery::tests {
namespace {

using RoomMap =
    std::unordered_map<snapshot::CatId, snapshot::RoomId>;

Result<workflow::PreviewBundle> Preview(
    WorkflowReadFake& reader,
    std::uint64_t generation) {
    workflow::WorkflowStateMachine state;
    AC_CHECK(state.BeginPreview());
    return workflow::PreviewBuilder(reader).Build(
        generation,
        workflow::WorkflowCapability::MoveOnly,
        state);
}

RoomMap FinalRooms(const workflow::PreviewBundle& bundle) {
    RoomMap result;
    for (const auto& cat : bundle.snapshot.cats) {
        AC_CHECK(cat.room_id.has_value());
        result.emplace(cat.id, *cat.room_id);
    }
    for (const auto& move : bundle.room_plan.moves) {
        result.at(move.cat_id) = move.to_room;
    }
    return result;
}

void ApplyRooms(snapshot::HouseSnapshot& house, const RoomMap& rooms) {
    for (auto& room : house.rooms) {
        room.residents.clear();
    }
    for (auto& cat : house.cats) {
        cat.room_id = rooms.at(cat.id);
        const auto room = std::ranges::find_if(
            house.rooms,
            [&cat](const auto& candidate) {
                return candidate.id == *cat.room_id;
            });
        AC_CHECK(room != house.rooms.end());
        room->residents.push_back(cat.id);
    }
}

snapshot::CatId FirstCatIn(
    const snapshot::HouseSnapshot& house,
    const snapshot::RoomId& room_id) {
    const auto room = std::ranges::find_if(
        house.rooms,
        [&room_id](const auto& candidate) {
            return candidate.id == room_id;
        });
    AC_CHECK(room != house.rooms.end());
    AC_CHECK(!room->residents.empty());
    return room->residents.front();
}

void MoveCat(
    snapshot::HouseSnapshot& house,
    snapshot::CatId cat_id,
    const snapshot::RoomId& target) {
    RoomMap rooms;
    for (const auto& cat : house.cats) {
        rooms.emplace(cat.id, *cat.room_id);
    }
    rooms.at(cat_id) = target;
    ApplyRooms(house, rooms);
}

}  // namespace

void RunDeterministicRoomAssignmentTests() {
    WorkflowReadFake reader;
    reader.house = WorkflowHouse(25);
    reader.house.rooms.front().id = "Floor1_Large";
    for (auto& cat : reader.house.cats) {
        cat.room_id = "Floor1_Large";
        cat.sex = cat.id <= 2
            ? snapshot::CatSex::Female
            : snapshot::CatSex::Male;
    }
    reader.house.rooms.push_back({.id = "Attic"});
    reader.house.rooms.push_back({.id = "Floor1_Small"});
    reader.house.rooms[0].attributes = snapshot::RoomAttributes{
        .comfort = 8,
        .stimulation = 4,
        .health = 2,
        .mutation = 1
    };
    reader.house.rooms[1].attributes = snapshot::RoomAttributes{};
    reader.house.rooms[2].attributes = snapshot::RoomAttributes{};
    reader.house.capabilities.read_room_attributes = true;

    const auto initial = Preview(reader, 40);
    AC_CHECK(static_cast<bool>(initial));
    const auto canonical = FinalRooms(initial.value);
    ApplyRooms(reader.house, canonical);

    std::unordered_map<snapshot::RoomId, std::size_t> counts;
    for (const auto& [cat_id, room_id] : canonical) {
        (void)cat_id;
        ++counts[room_id];
    }
    AC_CHECK(counts["Attic"] == 8);
    AC_CHECK(counts["Floor1_Large"] == 9);
    AC_CHECK(counts["Floor1_Small"] == 8);
    for (const auto& decision : initial.value.classification.decisions) {
        if (decision.primary_role ==
            classification::CatRole::CombatRecommended) {
            AC_CHECK(canonical.at(decision.cat_id) == "Floor1_Large");
        }
    }

    const auto from_large = FirstCatIn(reader.house, "Floor1_Large");
    const auto from_attic = FirstCatIn(reader.house, "Attic");
    MoveCat(reader.house, from_large, "Attic");
    MoveCat(reader.house, from_attic, "Floor1_Small");
    const auto repaired = Preview(reader, 41);
    AC_CHECK(static_cast<bool>(repaired));
    AC_CHECK(!repaired.value.room_plan.moves.empty());
    AC_CHECK(FinalRooms(repaired.value) == canonical);

    ApplyRooms(reader.house, canonical);
    const auto from_small = FirstCatIn(reader.house, "Floor1_Small");
    MoveCat(reader.house, from_large, "Floor1_Small");
    MoveCat(reader.house, from_small, "Floor1_Large");
    const auto swapped = Preview(reader, 42);
    AC_CHECK(static_cast<bool>(swapped));
    AC_CHECK(!swapped.value.room_plan.moves.empty());
    AC_CHECK(FinalRooms(swapped.value) == canonical);

    WorkflowReadFake breeding_reader;
    breeding_reader.house = WorkflowHouse(6);
    breeding_reader.house.rooms.front().id = "Floor1_Large";
    breeding_reader.house.rooms.front().attributes =
        snapshot::RoomAttributes{.comfort = 20, .stimulation = 1};
    breeding_reader.house.rooms.push_back({
        .id = "Attic",
        .attributes = snapshot::RoomAttributes{
            .comfort = 1, .stimulation = 20
        }
    });
    breeding_reader.house.capabilities.read_room_attributes = true;
    breeding_reader.house.capabilities.read_sexuality = true;
    breeding_reader.house.capabilities.read_relationships = true;
    for (auto& cat : breeding_reader.house.cats) {
        cat.room_id = "Attic";
        cat.sex = cat.id == 1
            ? snapshot::CatSex::Female
            : snapshot::CatSex::Male;
        cat.sexuality = snapshot::CatSexuality::Straight;
        cat.sexuality_coefficient = 0.0;
        for (auto& stat : cat.genetic_stats.values) {
            stat = cat.id <= 2 ? 7 : 6;
        }
        breeding_reader.house.rooms[1].residents.push_back(cat.id);
    }
    breeding_reader.house.rooms[0].residents.clear();
    breeding_reader.house.pedigree_pair_coefficients.push_back({1, 2, 0.0});
    const auto breeding_preview = Preview(breeding_reader, 43);
    AC_CHECK(static_cast<bool>(breeding_preview));
    const auto breeding_rooms = FinalRooms(breeding_preview.value);
    AC_CHECK(breeding_rooms.at(1) == "Attic");
    AC_CHECK(breeding_rooms.at(2) == "Attic");
}

}  // namespace autocattery::tests
