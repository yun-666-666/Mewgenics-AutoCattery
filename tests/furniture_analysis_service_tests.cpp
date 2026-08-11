#include "auto_cattery/furniture_analysis/service.hpp"

#include <algorithm>
#include <string>
#include <utility>

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

class FakeFurnitureAnalysisSource final
    : public furniture_analysis::IFurnitureAnalysisSource {
public:
    Result<furniture_analysis::FurnitureAnalysisSourceSnapshot> Capture(
        std::uint64_t generation) override {
        ++capture_calls;
        value.house.scene_generation = generation;
        return {value};
    }

    int capture_calls{};
    furniture_analysis::FurnitureAnalysisSourceSnapshot value;
};

snapshot::detail::FurniturePlacement Furniture(
    std::int64_t instance_id,
    std::string item_id,
    std::string room_id) {
    return {
        .instance_id = instance_id,
        .format_version = 1,
        .item_id = std::move(item_id),
        .room_id = std::move(room_id),
        .position_x = static_cast<std::int32_t>(instance_id),
        .position_y = 0,
        .position_z = 1,
        .scale_x = 1,
        .scale_y = 1};
}

}  // namespace

void RunFurnitureAnalysisServiceTests() {
    for (const std::size_t room_count : {1U, 2U, 3U, 5U, 7U}) {
        FakeFurnitureAnalysisSource source;
        source.value.house.snapshot_id = 10;
        source.value.house.game_day = 20;
        source.value.house.source_save_name = "private-save.sav";
        source.value.house.cats = {{.id = 1}};
        source.value.available_room_count = room_count;
        for (std::size_t index = 0; index < room_count; ++index) {
            const auto id = "Room" + std::to_string(index + 1U);
            source.value.house.rooms.push_back({.id = id});
            source.value.furniture.push_back(Furniture(
                static_cast<std::int64_t>(index + 1U), "chair", id));
        }
        source.value.furniture.push_back(Furniture(100, "chair", ""));
        source.value.furniture_info.records.push_back({.item_id = "chair"});
        source.value.furniture_effects.emplace(
            "chair", snapshot::RoomAttributes{.comfort = 2});

        furniture_analysis::FurnitureAnalysisService service(source);
        AC_CHECK(source.capture_calls == 0);
        const auto first = service.Analyze(77);
        AC_CHECK(static_cast<bool>(first));
        AC_CHECK(source.capture_calls == 1);
        AC_CHECK(first.value.rooms.size() == room_count);
        AC_CHECK(first.value.identified_room_count == room_count);
        AC_CHECK(first.value.furniture_count == room_count + 1U);
        AC_CHECK(first.value.placed_furniture_count == room_count);
        AC_CHECK(first.value.warehouse_furniture_count == 1U);
        AC_CHECK(first.value.furniture_info_coverage == room_count + 1U);
        AC_CHECK(first.value.furniture_effect_coverage == room_count + 1U);
        const auto second = service.Analyze(77);
        AC_CHECK(static_cast<bool>(second));
        AC_CHECK(source.capture_calls == 2);
        AC_CHECK(first.value.binding_digest == second.value.binding_digest);
    }

    FakeFurnitureAnalysisSource refreshed_layout;
    refreshed_layout.value.house.source_save_name = "current-layout.sav";
    refreshed_layout.value.available_room_count = 1;
    refreshed_layout.value.house.rooms = {{.id = "Attic"}};
    refreshed_layout.value.runtime_room_grids.push_back({
        "Attic", 2, 1, {2U, 0U}, {2U, 1U}});
    furniture_analysis::FurnitureAnalysisService refreshed_service(
        refreshed_layout);
    const auto before_manual_move = refreshed_service.Analyze(92);
    AC_CHECK(static_cast<bool>(before_manual_move));
    refreshed_layout.value.runtime_room_grids.front().live_cells[1] = 2U;
    const auto after_manual_move = refreshed_service.Analyze(92);
    AC_CHECK(static_cast<bool>(after_manual_move));
    AC_CHECK(refreshed_layout.capture_calls == 2);
    AC_CHECK(before_manual_move.value.binding_digest !=
        after_manual_move.value.binding_digest);

    FakeFurnitureAnalysisSource anonymous;
    anonymous.value.house.source_save_name = "anonymous.sav";
    anonymous.value.house.cats = {{.id = 1}};
    anonymous.value.available_room_count = 5;
    anonymous.value.house.rooms = {{.id = "KnownA"}, {.id = "KnownB"}};
    furniture_analysis::FurnitureAnalysisService service(anonymous);
    const auto result = service.Analyze(91);
    AC_CHECK(static_cast<bool>(result));
    AC_CHECK(result.value.rooms.size() == 5);
    AC_CHECK(result.value.identified_room_count == 2);
    AC_CHECK(!result.value.rooms.back().room_id.has_value());

    const auto invalid_generation = service.Analyze(0);
    AC_CHECK(!static_cast<bool>(invalid_generation));
    AC_CHECK(anonymous.capture_calls == 1);

    FakeFurnitureAnalysisSource upgrades;
    upgrades.value.house.source_save_name = "attribute-upgrades.sav";
    upgrades.value.available_room_count = 2;
    upgrades.value.house.rooms = {{.id = "RoomA"}, {.id = "RoomB"}};
    upgrades.value.furniture = {
        Furniture(1, "weak-one", "RoomA"),
        Furniture(2, "weak-two", "RoomB"),
        Furniture(3, "strong-one", ""),
        Furniture(4, "strong-two", ""),
        Furniture(5, "tradeoff", "")};
    upgrades.value.furniture_effects = {
        {"weak-one", snapshot::RoomAttributes{
            .comfort = 1, .stimulation = 1, .health = 1,
            .mutation = 1, .appeal = 1}},
        {"weak-two", snapshot::RoomAttributes{
            .comfort = 2, .stimulation = 2, .health = 2,
            .mutation = 2, .appeal = 2}},
        {"strong-one", snapshot::RoomAttributes{
            .comfort = 5, .stimulation = 5, .health = 5,
            .mutation = 5, .appeal = 5}},
        {"strong-two", snapshot::RoomAttributes{
            .comfort = 4, .stimulation = 4, .health = 4,
            .mutation = 4, .appeal = 4}},
        {"tradeoff", snapshot::RoomAttributes{
            .comfort = 100, .stimulation = 0, .health = 100,
            .mutation = 100, .appeal = 100}}};
    upgrades.value.runtime_scene_piece_count = 3;
    upgrades.value.runtime_placed_piece_count = 2;
    upgrades.value.runtime_warehouse_pieces = {
        {.stable_key = 3, .item_id = "strong-one"}};
    furniture_analysis::FurnitureAnalysisService upgrade_service(upgrades);
    const auto upgrade_result = upgrade_service.Analyze(93);
    AC_CHECK(static_cast<bool>(upgrade_result));
    AC_CHECK(upgrade_result.value.attribute_upgrades.size() == 2);
    AC_CHECK(upgrade_result.value.attribute_upgrades[0].warehouse_stable_key == 3);
    AC_CHECK(upgrade_result.value.attribute_upgrades[0].placed_stable_key == 1);
    AC_CHECK(upgrade_result.value.attribute_upgrades[1].warehouse_stable_key == 4);
    AC_CHECK(upgrade_result.value.attribute_upgrades[1].placed_stable_key == 2);
    AC_CHECK(upgrade_result.value.attribute_upgrade_gain.comfort == 6);
    AC_CHECK(upgrade_result.value.attribute_upgrade_gain.stimulation == 6);
    AC_CHECK(upgrade_result.value.attribute_upgrade_gain.health == 6);
    AC_CHECK(upgrade_result.value.attribute_upgrade_gain.mutation == 6);
    AC_CHECK(upgrade_result.value.attribute_upgrade_gain.appeal == 6);
    AC_CHECK(upgrade_result.value.runtime_scene_piece_count == 3);
    AC_CHECK(upgrade_result.value.runtime_placed_piece_count == 2);
    AC_CHECK(upgrade_result.value.runtime_warehouse_piece_count == 1);
    AC_CHECK(upgrade_result.value.runtime_warehouse_piece_match_count == 1);
    AC_CHECK(upgrade_result.value.layout_plan.warehouse_furniture_count == 3);
    AC_CHECK(std::ranges::none_of(
        upgrade_result.value.attribute_upgrades,
        [](const auto& upgrade) {
            return upgrade.warehouse_item_id == "tradeoff";
        }));
}

}  // namespace autocattery::tests
