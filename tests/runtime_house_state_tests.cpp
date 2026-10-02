#include "runtime_house_state.hpp"
#include "runtime_house_move_gateway.hpp"
#include "runtime_house_state_capture.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include "mew_ui_house_move_adapter.h"

#include "test_support.hpp"
#include "workflow_test_fixture.hpp"
#include "auto_cattery/workflow/preview_builder.hpp"

namespace autocattery::tests {
namespace {

snapshot::HouseSnapshot TwoRoomSnapshot() {
    snapshot::HouseSnapshot snapshot;
    snapshot.capabilities.read_room_assignments = true;
    snapshot.cats = {
        {.id = 1, .room_id = "Floor1_Large"},
        {.id = 2, .room_id = "Floor1_Large"},
        {.id = 3, .room_id = "Attic"},
        {.id = 4, .room_id = "Attic"}
    };
    snapshot.rooms = {
        {.id = "Floor1_Large", .residents = {1, 2}},
        {.id = "Attic", .residents = {3, 4}}
    };
    return snapshot;
}

ui::RuntimeHouseState RuntimeState(
    ui::RuntimePointer first_room_for_cat_one) {
    return {
        .available_room_count = 2,
        .cats = {
            {1, 1001, first_room_for_cat_one},
            {2, 1002, 100},
            {3, 1003, 200},
            {4, 1004, 200}
        },
        .rooms = {
            {100, {"Floor1_Large", "Attic"}},
            {200, {"Attic"}}
        }
    };
}

const snapshot::RoomSnapshot& Room(
    const snapshot::HouseSnapshot& snapshot,
    std::string_view id) {
    const auto room = std::ranges::find_if(
        snapshot.rooms,
        [id](const auto& candidate) { return candidate.id == id; });
    AC_CHECK(room != snapshot.rooms.end());
    return *room;
}

}  // namespace

void RunRuntimeHouseStateTests() {
    // Delivery reallocations must not make neighboring names into room IDs.
    alignas(8) std::array<std::uint8_t, 0x400> native_room{};
    const auto set_name = [&](const char* name) {
        std::memset(native_room.data() + 0x40, 0, 0x20);
        std::memcpy(native_room.data() + 0x40, name, std::strlen(name));
        const std::size_t size = std::strlen(name), capacity = 15;
        std::memcpy(native_room.data() + 0x50, &size, sizeof(size));
        std::memcpy(native_room.data() + 0x58, &capacity, sizeof(capacity));
    };
    set_name("Attic");
    std::memcpy(native_room.data() + 0x90, "Floor1_Large", 12);
    const char* stray = "Floor1_Small";
    std::memcpy(native_room.data() + 0x120, &stray, sizeof(stray));
    AC_CHECK(AcMewDetectNativeHouseRoomMask(native_room.data()) == (1U << 3));
    set_name("SpecialRoom");
    AC_CHECK(AcMewDetectNativeHouseRoomMask(native_room.data()) == 0);
    set_name("Floor2_Small");
    AC_CHECK(AcMewDetectNativeHouseRoomMask(native_room.data()) == (1U << 5));
    set_name("HousePipe");
    AC_CHECK(AcMewDetectNativeHouseRoomMask(native_room.data()) == (1U << 6));
    set_name("Floor2_Small");
    // The same native MSVC string can use heap storage.
    const std::size_t capacity = 31;
    std::memcpy(native_room.data() + 0x40, &stray, sizeof(stray));
    std::memcpy(native_room.data() + 0x58, &capacity, sizeof(capacity));
    AC_CHECK(AcMewDetectNativeHouseRoomMask(native_room.data()) == (1U << 1));
    const std::array<ui::RuntimeCatRoomState, 4> occupied_rooms{{
        {1, 1, 100},
        {2, 2, 100},
        {3, 3, 200},
        {4, 4, 300}
    }};
    AC_CHECK(ui::InferAvailableRoomCount(0, occupied_rooms) == 3);
    AC_CHECK(ui::InferAvailableRoomCount(5, occupied_rooms) == 3);
    AC_CHECK(ui::InferAvailableRoomCount(7, occupied_rooms) == 5);

    room_planning::RoomPlan no_op;
    no_op.fully_satisfied = true;
    no_op.disposition = room_planning::PlanDisposition::Complete;
    AC_CHECK(ui::IsRuntimeMovePlanApproved(no_op));
    no_op.validation_errors.push_back("synthetic-error");
    AC_CHECK(!ui::IsRuntimeMovePlanApproved(no_op));
    AC_CHECK(ui::RuntimeHouseMoveBatchSize(0) == 0);
    AC_CHECK(ui::RuntimeHouseMoveBatchSize(8) == 8);
    AC_CHECK(ui::RuntimeHouseMoveBatchSize(43) == 8);

    const auto original = TwoRoomSnapshot();
    const auto unchanged = RuntimeState(100);
    AC_CHECK(ui::RuntimeHouseStateMatches(original, unchanged));

    auto interrupted_snapshot = original;
    std::erase_if(interrupted_snapshot.cats, [](const auto& cat) { return cat.id == 2; });
    interrupted_snapshot.rooms[0].residents = {1};
    auto interrupted_runtime = unchanged;
    std::erase_if(interrupted_runtime.cats, [](const auto& cat) { return cat.cat_id == 2; });
    interrupted_runtime.rooms.push_back({300, {}}); // Actual special room.
    AC_CHECK(static_cast<bool>(ui::OverlayRuntimeHouseState(interrupted_snapshot, interrupted_runtime)));

    auto saved_pipe = original;
    saved_pipe.cats.front().room_id = "HousePipe";
    saved_pipe.rooms.front().residents = {2};
    saved_pipe.rooms.push_back({.id = "HousePipe", .residents = {1}});
    auto pipe_runtime = unchanged;
    pipe_runtime.cats.front().room = 300;
    pipe_runtime.rooms.push_back({300, {"HousePipe"}});
    AC_CHECK(static_cast<bool>(ui::OverlayRuntimeHouseState(saved_pipe, pipe_runtime)));
    AC_CHECK(saved_pipe.rooms.size() == 2);
    AC_CHECK(!saved_pipe.cats.front().room_id);
    AC_CHECK(ui::RuntimeHouseStateMatches(saved_pipe, pipe_runtime));
    // The cat may enter the pipe after the mapping was cached, or return home
    // while the save still records HousePipe. Both use the live position.
    auto cached_pipe = original;
    const auto ordinary_mapping = ui::ResolveRuntimeRoomPointers(original, unchanged);
    AC_CHECK(static_cast<bool>(ui::OverlayRuntimeHouseState(cached_pipe, pipe_runtime,
        ordinary_mapping.value)));
    AC_CHECK(!cached_pipe.cats.front().room_id);
    saved_pipe.rooms.push_back({.id = "HousePipe"});
    AC_CHECK(static_cast<bool>(ui::OverlayRuntimeHouseState(saved_pipe, unchanged)));
    AC_CHECK(saved_pipe.cats.front().room_id == "Floor1_Large");
    AC_CHECK(saved_pipe.rooms.size() == 2);
    auto unknown_pipe = pipe_runtime;
    unknown_pipe.rooms.back().detected_ids.clear();
    AC_CHECK(!ui::ResolveRuntimeRoomPointers(original, unknown_pipe));

    WorkflowReadFake pipe_reader;
    pipe_reader.house = WorkflowHouse(8);
    pipe_reader.house.rooms.front().id = "Floor1_Large";
    for (auto& cat : pipe_reader.house.cats) cat.room_id = "Floor1_Large";
    pipe_reader.house.rooms.push_back({.id = "Attic"});
    pipe_reader.house.rooms.push_back({.id = "HousePipe", .residents = {1}});
    pipe_reader.house.cats.front().room_id = "HousePipe";
    ui::RuntimeHouseState preview_runtime{.available_room_count = 2,
        .rooms = {{100, {"Floor1_Large"}}, {200, {"Attic"}}, {300, {"HousePipe"}}}};
    for (const auto& cat : pipe_reader.house.cats)
        preview_runtime.cats.push_back({cat.id, static_cast<ui::RuntimePointer>(1000 + cat.id),
            cat.id == 1 ? 300U : 100U});
    AC_CHECK(static_cast<bool>(ui::OverlayRuntimeHouseState(pipe_reader.house, preview_runtime)));
    workflow::WorkflowStateMachine pipe_preview_state;
    AC_CHECK(pipe_preview_state.BeginPreview());
    const auto pipe_preview = workflow::PreviewBuilder(pipe_reader).Build(
        50, workflow::WorkflowCapability::MoveOnly, pipe_preview_state);
    AC_CHECK(static_cast<bool>(pipe_preview));
    if (pipe_preview) {
        AC_CHECK(pipe_preview.value.room_plan.move_execution_allowed);
        const auto return_move = std::ranges::find(
            pipe_preview.value.room_plan.moves, 1, &room_planning::PlannedMove::cat_id);
        AC_CHECK(return_move != pipe_preview.value.room_plan.moves.end());
        if (return_move != pipe_preview.value.room_plan.moves.end()) {
            AC_CHECK(return_move->executable && return_move->from_room == "Outside");
            AC_CHECK(return_move->to_room != "HousePipe");
        }
    }

    const auto manually_moved = RuntimeState(200);
    AC_CHECK(!ui::RuntimeHouseStateMatches(original, manually_moved));
    auto overlaid = original;
    AC_CHECK(static_cast<bool>(
        ui::OverlayRuntimeHouseState(overlaid, manually_moved)));
    AC_CHECK(ui::RuntimeHouseStateMatches(overlaid, manually_moved));
    AC_CHECK(Room(overlaid, "Floor1_Large").residents.size() == 1);
    AC_CHECK(Room(overlaid, "Attic").residents.size() == 3);

    const auto fixed_mapping =
        ui::ResolveRuntimeRoomPointers(original, unchanged);
    AC_CHECK(static_cast<bool>(fixed_mapping));
    auto swapped_runtime = unchanged;
    swapped_runtime.cats[0].room = 200;
    swapped_runtime.cats[1].room = 200;
    swapped_runtime.cats[2].room = 100;
    swapped_runtime.cats[3].room = 100;
    auto stable_overlay = original;
    AC_CHECK(static_cast<bool>(ui::OverlayRuntimeHouseState(
        stable_overlay, swapped_runtime, fixed_mapping.value)));
    AC_CHECK(stable_overlay.cats[0].room_id == "Attic");
    AC_CHECK(stable_overlay.cats[2].room_id == "Floor1_Large");

    auto outside = unchanged;
    outside.cats.front().room = 0;
    auto outside_overlay = original;
    AC_CHECK(static_cast<bool>(
        ui::OverlayRuntimeHouseState(outside_overlay, outside)));
    AC_CHECK(!outside_overlay.cats.front().room_id.has_value());
    AC_CHECK(Room(outside_overlay, "Floor1_Large").residents.size() == 1);
    AC_CHECK(ui::RuntimeHouseStateMatches(outside_overlay, outside));

    // Regression: furniture adds an empty room to the save snapshot.
    // Occupied-cat pointers alone cannot resolve it; native room evidence must
    // include it, and overlay must keep both its identity and attributes.
    auto with_empty = original;
    with_empty.rooms.push_back({.id = "Floor1_Small",
        .attributes = snapshot::RoomAttributes{.comfort = -14}});
    auto empty_runtime = unchanged;
    AC_CHECK(!ui::ResolveRuntimeRoomPointers(with_empty, empty_runtime));
    empty_runtime.available_room_count = 3;
    empty_runtime.rooms.push_back({300, {"Floor1_Small"}});
    const auto empty_mapping = ui::ResolveRuntimeRoomPointers(with_empty, empty_runtime);
    AC_CHECK(static_cast<bool>(empty_mapping));
    AC_CHECK(empty_mapping.value.at("Floor1_Small") == 300);
    AC_CHECK(static_cast<bool>(ui::OverlayRuntimeHouseState(with_empty, empty_runtime)));
    AC_CHECK(Room(with_empty, "Floor1_Small").residents.empty());
    AC_CHECK(Room(with_empty, "Floor1_Small").attributes->comfort == -14);

    auto five_rooms = with_empty;
    five_rooms.rooms.push_back({.id = "Floor2_Large"});
    five_rooms.rooms.push_back({.id = "Floor2_Small"});
    auto five_runtime = empty_runtime;
    five_runtime.available_room_count = 5;
    five_runtime.rooms.push_back({400, {"Floor2_Large"}});
    five_runtime.rooms.push_back({500, {"Floor2_Small"}});
    const auto five_mapping = ui::ResolveRuntimeRoomPointers(five_rooms, five_runtime);
    AC_CHECK(static_cast<bool>(five_mapping));
    AC_CHECK(five_mapping.value.at("Floor2_Small") == 500);
    AC_CHECK(static_cast<bool>(ui::OverlayRuntimeHouseState(five_rooms, five_runtime)));
    AC_CHECK(five_rooms.rooms.size() == 5);
    auto five_pipe = five_runtime;
    five_pipe.cats.front().room = 600;
    five_pipe.rooms.push_back({600, {"HousePipe"}});
    auto five_pipe_snapshot = five_rooms;
    five_pipe_snapshot.rooms.push_back({.id = "HousePipe", .residents = {1}});
    AC_CHECK(static_cast<bool>(ui::OverlayRuntimeHouseState(five_pipe_snapshot, five_pipe)));
    AC_CHECK(five_pipe_snapshot.rooms.size() == 5);
    AC_CHECK(!five_pipe_snapshot.cats.front().room_id);

    auto changed_identity = unchanged;
    changed_identity.cats.back().cat_id = 99;
    AC_CHECK(!static_cast<bool>(
        ui::OverlayRuntimeHouseState(overlaid, changed_identity)));

    auto ambiguous = original;
    for (auto& cat : ambiguous.cats) {
        cat.room_id.reset();
    }
    for (auto& room : ambiguous.rooms) {
        room.residents.clear();
    }
    auto no_evidence = unchanged;
    no_evidence.rooms.clear();
    AC_CHECK(!static_cast<bool>(
        ui::ResolveRuntimeRoomPointers(ambiguous, no_evidence)));
}

}  // namespace autocattery::tests
