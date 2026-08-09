#include "auto_cattery/furniture_analysis/service.hpp"

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
        AC_CHECK(first.value.binding_digest == second.value.binding_digest);
    }

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
}

}  // namespace autocattery::tests
