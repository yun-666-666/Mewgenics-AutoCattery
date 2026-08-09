#include "auto_cattery/furniture_analysis/service.hpp"

#include <algorithm>
#include <cstddef>
#include <map>
#include <sstream>
#include <unordered_set>

#include "auto_cattery/workflow/digests.hpp"

namespace autocattery::furniture_analysis {
namespace {

void Add(
    snapshot::RoomAttributes& total,
    const snapshot::RoomAttributes& value) {
    total.comfort += value.comfort;
    total.stimulation += value.stimulation;
    total.health += value.health;
    total.mutation += value.mutation;
    total.appeal += value.appeal;
}

template<class T>
void Append(std::ostringstream& output, const T& value) {
    output << value << '|';
}

std::string BuildBindingDigest(
    const FurnitureAnalysisSourceSnapshot& source,
    const std::vector<FurnitureAnalysisRoom>& rooms) {
    std::ostringstream canonical;
    Append(canonical, source.house.scene_generation);
    Append(canonical, source.house.game_day.value_or(-1));
    Append(
        canonical,
        workflow::DigestPrivateIdentity(source.house.source_save_name));
    for (const auto& cat : source.house.cats) {
        Append(canonical, cat.id);
        Append(canonical, cat.room_id.value_or(""));
    }
    for (const auto& room : rooms) {
        Append(canonical, room.binding_key);
        Append(canonical, room.resident_count);
        Append(canonical, room.furniture_count);
        Append(canonical, room.attributes.comfort);
        Append(canonical, room.attributes.stimulation);
        Append(canonical, room.attributes.health);
        Append(canonical, room.attributes.mutation);
        Append(canonical, room.attributes.appeal);
    }
    for (const auto& item : source.furniture) {
        Append(canonical, item.instance_id);
        Append(canonical, item.format_version);
        Append(canonical, item.item_id);
        Append(canonical, item.unknown_after_item_length);
        Append(canonical, item.unknown_before_room);
        Append(canonical, item.room_id);
        Append(canonical, item.unknown_after_room_length);
        Append(canonical, item.position_x);
        Append(canonical, item.position_y);
        Append(canonical, item.position_z);
        Append(canonical, item.unknown_flag_1);
        Append(canonical, item.unknown_flag_2);
    }
    for (const auto& room : source.geometry.rooms) {
        Append(canonical, room.definition_id);
        Append(canonical, room.room_id);
        Append(canonical, room.width);
        Append(canonical, room.height);
        for (const auto& row : room.built_in_collision) {
            for (const auto value : row) {
                Append(canonical, value);
            }
        }
    }
    for (const auto& house : source.geometry.houses) {
        Append(canonical, house.house_id);
        for (const auto& room : house.room_positions) {
            Append(canonical, room.room_definition_id);
            Append(canonical, room.room_id);
            Append(canonical, room.x);
            Append(canonical, room.y);
        }
    }
    for (const auto& item : source.furniture_info.records) {
        Append(canonical, item.item_id);
        Append(canonical, item.unknown_after_name_length);
        for (const auto byte : item.opaque_payload) {
            Append(canonical, std::to_integer<unsigned int>(byte));
        }
    }
    std::vector<std::pair<std::string, snapshot::RoomAttributes>> effects{
        source.furniture_effects.begin(), source.furniture_effects.end()};
    std::ranges::sort(effects, {}, &decltype(effects)::value_type::first);
    for (const auto& [item_id, value] : effects) {
        Append(canonical, item_id);
        Append(canonical, value.comfort);
        Append(canonical, value.stimulation);
        Append(canonical, value.health);
        Append(canonical, value.mutation);
        Append(canonical, value.appeal);
    }
    return workflow::DigestPrivateIdentity(canonical.str());
}

}  // namespace

FurnitureAnalysisService::FurnitureAnalysisService(
    IFurnitureAnalysisSource& source)
    : source_(source) {}

Result<FurnitureAnalysisSnapshot> FurnitureAnalysisService::Analyze(
    std::uint64_t scene_generation) {
    if (scene_generation == 0U) {
        return {{}, ErrorCode::SceneUnavailable,
                "furniture analysis requires a House generation"};
    }
    auto captured = source_.Capture(scene_generation);
    if (!captured) {
        return {{}, captured.code, captured.message};
    }
    auto& source = captured.value;
    if (source.house.scene_generation != scene_generation) {
        return {{}, ErrorCode::SnapshotInvalid,
                "furniture analysis generation changed during capture"};
    }

    std::map<snapshot::RoomId, FurnitureAnalysisRoom> identified;
    for (const auto& room : source.house.rooms) {
        if (room.id == "AdventureBox") {
            continue;
        }
        auto& analysis = identified[room.id];
        analysis.binding_key = "id:" + room.id;
        analysis.room_id = room.id;
        analysis.resident_count = room.residents.size();
    }
    for (const auto& room_id : source.runtime_detected_room_ids) {
        if (room_id.empty() || room_id == "AdventureBox") {
            continue;
        }
        auto& analysis = identified[room_id];
        analysis.binding_key = "id:" + room_id;
        analysis.room_id = room_id;
    }

    std::unordered_set<std::string> info_ids;
    info_ids.reserve(source.furniture_info.records.size());
    for (const auto& item : source.furniture_info.records) {
        info_ids.insert(item.item_id);
    }

    FurnitureAnalysisSnapshot result;
    result.scene_generation = scene_generation;
    result.game_day = source.house.game_day;
    result.save_identity = workflow::DigestPrivateIdentity(
        source.house.source_save_name);
    result.cat_count = source.house.cats.size();
    result.furniture_count = source.furniture.size();
    for (const auto& item : source.furniture) {
        result.furniture_info_coverage +=
            info_ids.contains(item.item_id) ? 1U : 0U;
        result.furniture_effect_coverage +=
            source.furniture_effects.contains(item.item_id) ? 1U : 0U;
        if (item.room_id.empty()) {
            ++result.warehouse_furniture_count;
            continue;
        }
        ++result.placed_furniture_count;
        auto& room = identified[item.room_id];
        room.binding_key = "id:" + item.room_id;
        room.room_id = item.room_id;
        ++room.furniture_count;
        const auto effect = source.furniture_effects.find(item.item_id);
        if (effect != source.furniture_effects.end()) {
            Add(room.attributes, effect->second);
        }
    }

    result.identified_room_count = identified.size();
    result.rooms.reserve(std::max(
        identified.size(), source.available_room_count));
    for (auto& [id, room] : identified) {
        (void)id;
        result.rooms.push_back(std::move(room));
    }
    for (std::size_t index = result.rooms.size();
         index < source.available_room_count;
         ++index) {
        result.rooms.push_back({
            .binding_key = "anonymous-runtime-room:" +
                std::to_string(index - result.identified_room_count + 1U)});
    }
    if (result.rooms.empty()) {
        return {{}, ErrorCode::RoomDataUnavailable,
                "furniture analysis found no current House rooms"};
    }
    result.binding_digest = BuildBindingDigest(source, result.rooms);
    return {std::move(result)};
}

}  // namespace autocattery::furniture_analysis
