#include "auto_cattery/snapshot/house_state_writer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <string_view>
#include <unordered_set>

namespace autocattery::snapshot {
namespace {

template<class T>
void Append(std::vector<std::uint8_t>& bytes, const T& value) {
    const auto* begin = reinterpret_cast<const std::uint8_t*>(&value);
    bytes.insert(bytes.end(), begin, begin + sizeof(T));
}

bool SameEntry(const HouseStateEntry& left, const HouseStateEntry& right) {
    return left.cat_id == right.cat_id && left.room_id == right.room_id &&
        left.position_x == right.position_x &&
        left.position_y == right.position_y &&
        left.position_z == right.position_z;
}

bool IsOrdinaryRoom(const std::string& room_id) {
    constexpr std::array<std::string_view, 4> kVerifiedCurrentBuildRooms{
        "Floor1_Large", "Floor1_Small", "Floor2_Large", "Attic"};
    return std::ranges::find(kVerifiedCurrentBuildRooms, room_id) !=
        kVerifiedCurrentBuildRooms.end();
}

}  // namespace

Result<std::vector<std::uint8_t>> SerializeHouseState(
    std::span<const HouseStateEntry> entries) {
    if (entries.size() > 10'000 ||
        entries.size() > std::numeric_limits<std::uint32_t>::max()) {
        return {{}, ErrorCode::RoomDataUnavailable,
            "house_state entry count is invalid"};
    }
    std::unordered_set<CatId> ids;
    std::size_t total_size = sizeof(std::uint32_t) * 2;
    for (const auto& entry : entries) {
        if (entry.cat_id <= 0 || !ids.insert(entry.cat_id).second ||
            entry.room_id.size() > 128 ||
            entry.room_id.find('\0') != std::string::npos ||
            !std::isfinite(entry.position_x) ||
            !std::isfinite(entry.position_y) ||
            !std::isfinite(entry.position_z)) {
            return {{}, ErrorCode::RoomDataUnavailable,
                "house_state entry cannot be serialized safely"};
        }
        total_size += sizeof(entry.cat_id) + sizeof(std::uint64_t) +
            entry.room_id.size() + sizeof(double) * 3;
    }

    std::vector<std::uint8_t> bytes;
    bytes.reserve(total_size);
    Append(bytes, std::uint32_t{0});
    Append(bytes, static_cast<std::uint32_t>(entries.size()));
    for (const auto& entry : entries) {
        Append(bytes, entry.cat_id);
        Append(bytes, static_cast<std::uint64_t>(entry.room_id.size()));
        bytes.insert(bytes.end(), entry.room_id.begin(), entry.room_id.end());
        Append(bytes, entry.position_x);
        Append(bytes, entry.position_y);
        Append(bytes, entry.position_z);
    }
    return {std::move(bytes)};
}

Result<SingleCatTestRelocation> BuildSingleCatTestRelocation(
    std::span<const std::uint8_t> original_blob,
    std::size_t moved_index,
    std::size_t placement_source_index) {
    const auto parsed = ParseHouseState(original_blob);
    if (!parsed || moved_index >= parsed.value.size() ||
        placement_source_index >= parsed.value.size() ||
        moved_index == placement_source_index) {
        return {{}, ErrorCode::RoomDataUnavailable,
            "single-cat test placement indices are invalid"};
    }
    auto entries = parsed.value;
    const auto original = entries[moved_index];
    const auto& placement = entries[placement_source_index];
    if (!IsOrdinaryRoom(original.room_id) ||
        !IsOrdinaryRoom(placement.room_id) ||
        original.room_id == placement.room_id) {
        return {{}, ErrorCode::RoomDataUnavailable,
            "single-cat test requires two different ordinary rooms"};
    }

    entries[moved_index].room_id = placement.room_id;
    entries[moved_index].position_x = placement.position_x;
    entries[moved_index].position_y = placement.position_y;
    entries[moved_index].position_z = placement.position_z;
    const auto encoded = SerializeHouseState(entries);
    if (!encoded) {
        return {{}, encoded.code, encoded.message};
    }
    const auto readback = ParseHouseState(encoded.value);
    if (!readback || readback.value.size() != parsed.value.size()) {
        return {{}, ErrorCode::RoomDataUnavailable,
            "serialized house_state could not be read back"};
    }
    for (std::size_t index = 0; index < readback.value.size(); ++index) {
        const auto& expected = index == moved_index
            ? entries[index]
            : parsed.value[index];
        if (!SameEntry(readback.value[index], expected)) {
            return {{}, ErrorCode::RoomDataUnavailable,
                "house_state readback changed an unexpected entry"};
        }
    }
    return {{
        .encoded_house_state = encoded.value,
        .original = original,
        .relocated = entries[moved_index],
        .moved_index = moved_index,
        .placement_source_index = placement_source_index
    }};
}

}  // namespace autocattery::snapshot
