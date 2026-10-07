#include "auto_cattery/snapshot/detail/furniture_attributes.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <span>
#include <unordered_map>

namespace autocattery::snapshot::detail {
namespace {

template<class T>
bool Read(
    std::span<const std::byte> bytes,
    std::size_t offset,
    T& value) {
    if (offset > bytes.size() || bytes.size() - offset < sizeof(T)) {
        return false;
    }
    std::memcpy(&value, bytes.data() + offset, sizeof(T));
    return true;
}

bool ParsePlacement(
    std::span<const std::byte> bytes,
    FurniturePlacement& placement) {
    std::uint64_t item_length{};
    if (!Read(bytes, 4, item_length) || item_length > bytes.size()) {
        return false;
    }
    const std::size_t item_start = 12;
    const std::size_t header = item_start + item_length;
    std::uint32_t room_length{};
    if (header < item_start || !Read(bytes, header, placement.flags) ||
        !Read(bytes, header + 8, room_length)) {
        return false;
    }
    const std::size_t room_start = header + 16;
    if (room_start < header || room_length > bytes.size() - room_start) {
        return false;
    }
    placement.item_id.assign(
        reinterpret_cast<const char*>(bytes.data() + item_start),
        static_cast<std::size_t>(item_length));
    placement.room_id.assign(
        reinterpret_cast<const char*>(bytes.data() + room_start),
        room_length);
    return !placement.item_id.empty();
}

void Add(RoomAttributes& total, const RoomAttributes& value, double multiplier) {
    total.comfort += value.comfort * multiplier;
    total.stimulation += value.stimulation * multiplier;
    total.health += value.health * multiplier;
    total.mutation += value.mutation * multiplier;
    total.appeal += value.appeal * multiplier;
    total.breed_suppression += value.breed_suppression * multiplier;
}

}  // namespace

bool ParseFurniturePlacements(
    const std::vector<FurnitureStorageRecord>& records,
    std::vector<FurniturePlacement>& placements,
    std::string& error) {
    placements.clear();
    placements.reserve(records.size());
    for (const auto& record : records) {
        FurniturePlacement placement;
        if (!ParsePlacement(record.blob, placement)) {
            error = "furniture record has an unsupported layout";
            return false;
        }
        placements.push_back(std::move(placement));
    }
    return true;
}

bool ApplyUnlockedHouseRooms(HouseSnapshot& snapshot, std::span<const std::byte> bytes) {
    std::size_t cursor{};
    const auto read_string = [&](std::string& text) {
        std::uint64_t length{};
        if (!Read(bytes, cursor, length)) return false;
        cursor += sizeof(length);
        if (length > bytes.size() - cursor) return false;
        text.assign(reinterpret_cast<const char*>(bytes.data() + cursor),
                    static_cast<std::size_t>(length));
        cursor += static_cast<std::size_t>(length);
        return true;
    };
    std::uint32_t version{};
    if (!Read(bytes, cursor, version) || version != 1) return false;
    cursor += sizeof(version);
    std::string house;
    if (!read_string(house)) return false;
    std::uint64_t count{};
    if (!Read(bytes, cursor, count) || count > bytes.size() / 8) return false;
    cursor += sizeof(count);
    std::vector<RoomId> rooms;
    for (std::uint64_t index = 0; index < count; ++index) {
        std::string upgrade;
        if (!read_string(upgrade)) return false;
        // Upgrade-to-room mapping verified in current data/house.gon.
        const char* id = upgrade == "Default" ? "Floor1_Large" :
            upgrade == "SmallHouse_Attic" ? "Attic" :
            upgrade == "MediumHouse_SmallRoom" ? "Floor1_Small" :
            upgrade == "LargeHouse_Floor2Large" ? "Floor2_Large" :
            upgrade == "LargeHouse_Floor2Small" ? "Floor2_Small" : nullptr;
        if (id) rooms.emplace_back(id);
    }
    for (const auto& id : rooms) {
        if (std::ranges::none_of(snapshot.rooms, [&](const auto& room) {
                return room.id == id;
            })) {
            snapshot.rooms.push_back({.id = id});
        }
    }
    return true;
}

void ApplyFurnitureRoomAttributes(
    HouseSnapshot& snapshot,
    const std::vector<FurniturePlacement>& placements,
    const FurnitureCatalog& catalog) {
    // A furnished room remains available even when its last resident moved out.
    // Cat-derived house entries alone omit empty rooms.
    for (const auto& placement : placements) {
        const auto& id = placement.room_id;
        if (id != "Attic" && id != "Floor1_Large" &&
            id != "Floor1_Small" && id != "Floor2_Large" && id != "Floor2_Small") {
            continue;
        }
        if (std::ranges::none_of(snapshot.rooms, [&](const auto& room) {
                return room.id == id;
            })) {
            snapshot.rooms.push_back({.id = id});
        }
    }
    std::unordered_map<RoomId, RoomAttributes> totals;
    for (const auto& room : snapshot.rooms) {
        totals.emplace(room.id, RoomAttributes{});
    }
    for (const auto& placement : placements) {
        if (placement.item_id == "poop" || placement.room_id.empty() ||
            !totals.contains(placement.room_id)) {
            continue;
        }
        const auto found = catalog.find(placement.item_id);
        if (found == catalog.end()) {
            snapshot.capabilities.read_room_attributes = false;
            return;
        }
        // Native Rare repeats effects twice. Installed enhanced furniture uses
        // the same saved bits and repeats them 1 + ((flags & 6) >> 1) times.
        const auto multiplier = 1U + ((placement.flags & 0x6U) >> 1U);
        Add(totals.at(placement.room_id), found->second,
            static_cast<double>(multiplier));
    }
    for (auto& room : snapshot.rooms) {
        room.attributes = totals.at(room.id);
    }
    snapshot.capabilities.read_room_attributes = true;
}

}  // namespace autocattery::snapshot::detail
