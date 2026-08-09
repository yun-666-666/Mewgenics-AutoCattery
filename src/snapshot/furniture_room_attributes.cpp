#include "auto_cattery/snapshot/detail/furniture_attributes.hpp"

#include <cstdint>
#include <cstring>
#include <span>
#include <unordered_map>
#include <unordered_set>

namespace autocattery::snapshot::detail {
namespace {

template<class T>
bool Read(
    std::span<const std::byte> bytes,
    std::size_t& offset,
    T& value) {
    if (offset > bytes.size() || bytes.size() - offset < sizeof(T)) {
        return false;
    }
    std::memcpy(&value, bytes.data() + offset, sizeof(T));
    offset += sizeof(T);
    return true;
}

bool ReadString(
    std::span<const std::byte> bytes,
    std::size_t& offset,
    std::uint32_t& unknown_after_length,
    std::string& value) {
    std::uint32_t length{};
    if (!Read(bytes, offset, length) ||
        !Read(bytes, offset, unknown_after_length) ||
        length > bytes.size() - offset) {
        return false;
    }
    value.assign(
        reinterpret_cast<const char*>(bytes.data() + offset),
        static_cast<std::size_t>(length));
    offset += static_cast<std::size_t>(length);
    return true;
}

bool ParsePlacement(
    std::span<const std::byte> bytes,
    FurniturePlacement& placement) {
    std::size_t offset{};
    if (!Read(bytes, offset, placement.format_version) ||
        placement.format_version != 1U ||
        !ReadString(
            bytes, offset,
            placement.unknown_after_item_length,
            placement.item_id) ||
        placement.item_id.empty() ||
        !Read(bytes, offset, placement.placement_flags) ||
        !ReadString(
            bytes, offset,
            placement.unknown_after_room_length,
            placement.room_id) ||
        !Read(bytes, offset, placement.position_x) ||
        !Read(bytes, offset, placement.position_y) ||
        !Read(bytes, offset, placement.position_z) ||
        !Read(bytes, offset, placement.unknown_flag_1) ||
        !Read(bytes, offset, placement.unknown_flag_2)) {
        return false;
    }
    return offset == bytes.size();
}

void Add(RoomAttributes& total, const RoomAttributes& value) {
    total.comfort += value.comfort;
    total.stimulation += value.stimulation;
    total.health += value.health;
    total.mutation += value.mutation;
    total.appeal += value.appeal;
}

}  // namespace

bool ParseFurniturePlacements(
    const std::vector<FurnitureStorageRecord>& records,
    std::vector<FurniturePlacement>& placements,
    std::string& error) {
    placements.clear();
    placements.reserve(records.size());
    std::unordered_set<std::int64_t> instance_ids;
    for (const auto& record : records) {
        FurniturePlacement placement;
        placement.instance_id = record.key;
        if (!instance_ids.insert(record.key).second) {
            error = "furniture record has a duplicate instance key";
            return false;
        }
        if (!ParsePlacement(record.blob, placement)) {
            error = "furniture record has an unsupported layout";
            return false;
        }
        placements.push_back(std::move(placement));
    }
    return true;
}

void ApplyFurnitureRoomAttributes(
    HouseSnapshot& snapshot,
    const std::vector<FurniturePlacement>& placements,
    const FurnitureCatalog& catalog) {
    std::unordered_map<RoomId, RoomAttributes> totals;
    for (const auto& room : snapshot.rooms) {
        totals.emplace(room.id, RoomAttributes{});
    }
    for (const auto& placement : placements) {
        if (placement.room_id.empty() ||
            !totals.contains(placement.room_id)) {
            continue;
        }
        const auto found = catalog.find(placement.item_id);
        if (found == catalog.end()) {
            snapshot.capabilities.read_room_attributes = false;
            return;
        }
        Add(totals.at(placement.room_id), found->second);
    }
    for (auto& room : snapshot.rooms) {
        room.attributes = totals.at(room.id);
    }
    snapshot.capabilities.read_room_attributes = true;
}

}  // namespace autocattery::snapshot::detail
