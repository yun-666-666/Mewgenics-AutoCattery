#include "auto_cattery/snapshot/save_snapshot_adapter.hpp"

#include <cmath>
#include <cstring>

namespace autocattery::snapshot {
namespace {

template<class T>
bool Read(
    std::span<const std::uint8_t> bytes,
    std::size_t& cursor,
    T& value) {
    if (cursor > bytes.size() || sizeof(T) > bytes.size() - cursor) {
        return false;
    }
    std::memcpy(&value, bytes.data() + cursor, sizeof(T));
    cursor += sizeof(T);
    return true;
}

bool ReadString(
    std::span<const std::uint8_t> bytes,
    std::size_t& cursor,
    std::string& value,
    bool allow_empty = false) {
    std::uint64_t length{};
    if (!Read(bytes, cursor, length) ||
        (!allow_empty && length == 0) ||
        length > 128 ||
        length > bytes.size() - cursor) {
        return false;
    }
    value.assign(
        reinterpret_cast<const char*>(bytes.data() + cursor),
        static_cast<std::size_t>(length));
    cursor += static_cast<std::size_t>(length);
    return value.find('\0') == std::string::npos;
}

}  // namespace

Result<std::vector<HouseStateEntry>> ParseHouseState(
    std::span<const std::uint8_t> blob) {
    std::size_t cursor{};
    std::uint32_t format_version{};
    std::uint32_t count{};
    if (!Read(blob, cursor, format_version) ||
        !Read(blob, cursor, count) ||
        format_version != 0 ||
        count > 10'000) {
        return {
            {},
            ErrorCode::RoomDataUnavailable,
            "house_state header is invalid"
        };
    }

    std::vector<HouseStateEntry> entries;
    entries.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index) {
        HouseStateEntry entry;
        if (!Read(blob, cursor, entry.cat_id) ||
            entry.cat_id <= 0 ||
            !ReadString(blob, cursor, entry.room_id, true) ||
            !Read(blob, cursor, entry.position_x) ||
            !Read(blob, cursor, entry.position_y) ||
            !Read(blob, cursor, entry.position_z) ||
            !std::isfinite(entry.position_x) ||
            !std::isfinite(entry.position_y) ||
            !std::isfinite(entry.position_z)) {
            return {
                {},
                ErrorCode::RoomDataUnavailable,
                "house_state entry is invalid"
            };
        }
        entries.push_back(std::move(entry));
    }

    if (cursor != blob.size()) {
        return {
            {},
            ErrorCode::RoomDataUnavailable,
            "house_state has unexpected trailing bytes"
        };
    }
    return {std::move(entries)};
}

}  // namespace autocattery::snapshot
