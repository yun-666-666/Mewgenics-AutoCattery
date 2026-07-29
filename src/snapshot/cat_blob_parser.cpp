#include "auto_cattery/snapshot/save_snapshot_adapter.hpp"

#include <windows.h>

#include <cstring>

#include "auto_cattery/snapshot/detail/lz4_block.hpp"

namespace autocattery::snapshot {
namespace {

constexpr std::uint32_t kCatMagic = 19;
constexpr std::size_t kNameLengthOffset = 12;
constexpr std::size_t kNameStart = 20;
constexpr std::size_t kPreStringMetadataSize = 24;
constexpr std::size_t kEquipmentBlockSize = 368;
constexpr std::size_t kStatBlockSize = 92;
constexpr std::size_t kStatSeedSize = 8;
constexpr std::size_t kStatBonusOffset = 36;
constexpr std::size_t kStatEquipmentOffset = 64;
constexpr std::size_t kPreAbilityMetadataSize = 14;
constexpr std::size_t kAbilitySlotCount = 9;

template<class T>
bool ReadAt(
    std::span<const std::uint8_t> bytes,
    std::size_t offset,
    T& value) {
    if (offset > bytes.size() || sizeof(T) > bytes.size() - offset) {
        return false;
    }
    std::memcpy(&value, bytes.data() + offset, sizeof(T));
    return true;
}

bool ReadAsciiString(
    std::span<const std::uint8_t> bytes,
    std::size_t& cursor,
    std::string& value,
    bool allow_empty = false) {
    std::uint64_t length{};
    if (!ReadAt(bytes, cursor, length) ||
        (!allow_empty && length == 0) ||
        length > 1'000 ||
        sizeof(length) + length > bytes.size() - cursor) {
        return false;
    }
    cursor += sizeof(length);
    value.assign(
        reinterpret_cast<const char*>(bytes.data() + cursor),
        static_cast<std::size_t>(length));
    cursor += static_cast<std::size_t>(length);
    return value.find('\0') == std::string::npos;
}

Result<std::string> ReadDisplayName(
    std::span<const std::uint8_t> bytes,
    std::size_t& name_end) {
    std::uint32_t character_count{};
    if (!ReadAt(bytes, kNameLengthOffset, character_count) ||
        character_count > 100 ||
        character_count * 2U > bytes.size() - kNameStart) {
        return {{}, ErrorCode::CatDataUnavailable, "cat name is invalid"};
    }

    std::wstring wide;
    wide.resize(character_count);
    std::memcpy(
        wide.data(),
        bytes.data() + kNameStart,
        character_count * sizeof(wchar_t));
    const auto utf8_size = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        wide.data(),
        static_cast<int>(wide.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (utf8_size <= 0) {
        return {{}, ErrorCode::CatDataUnavailable, "cat name is not valid UTF-16"};
    }
    std::string utf8(static_cast<std::size_t>(utf8_size), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        wide.data(),
        static_cast<int>(wide.size()),
        utf8.data(),
        utf8_size,
        nullptr,
        nullptr);
    name_end = kNameStart + character_count * sizeof(wchar_t);
    return {std::move(utf8)};
}

bool ReadStats(
    std::span<const std::uint8_t> bytes,
    std::size_t block_start,
    std::size_t relative_offset,
    StatBlock& output) {
    for (std::size_t index = 0; index < kStatCount; ++index) {
        std::int32_t value{};
        if (!ReadAt(
                bytes,
                block_start + relative_offset + index * sizeof(value),
                value)) {
            return false;
        }
        output.values[index] = value;
    }
    return true;
}

}  // namespace

Result<CatSnapshot> ParseCatBlob(
    CatId cat_id,
    std::span<const std::uint8_t> blob,
    std::optional<std::int64_t> current_day) {
    if (cat_id <= 0) {
        return {{}, ErrorCode::CatDataUnavailable, "cat ID is invalid"};
    }
    const auto decoded = detail::DecodeCatStorageBlob(blob);
    if (!decoded) {
        return {{}, decoded.code, decoded.message};
    }
    const auto bytes = std::span<const std::uint8_t>(decoded.value);
    std::uint32_t magic{};
    if (!ReadAt(bytes, 0, magic) || magic != kCatMagic) {
        return {{}, ErrorCode::CatDataUnavailable, "cat magic is invalid"};
    }

    CatSnapshot cat;
    cat.id = cat_id;
    std::size_t cursor{};
    const auto name = ReadDisplayName(bytes, cursor);
    if (!name) {
        return {{}, name.code, name.message};
    }
    cat.display_name = name.value;
    if (kPreStringMetadataSize > bytes.size() - cursor) {
        return {{}, ErrorCode::CatDataUnavailable, "cat metadata is truncated"};
    }
    cursor += kPreStringMetadataSize;
    if (!ReadAsciiString(bytes, cursor, cat.breed_id) ||
        kEquipmentBlockSize > bytes.size() - cursor) {
        return {{}, ErrorCode::CatDataUnavailable, "cat breed block is invalid"};
    }
    cursor += kEquipmentBlockSize;
    if (!ReadAsciiString(bytes, cursor, cat.voice_id) ||
        kStatBlockSize > bytes.size() - cursor) {
        return {{}, ErrorCode::CatDataUnavailable, "cat voice/stat block is invalid"};
    }

    const auto stat_start = cursor;
    if (!ReadStats(bytes, stat_start, kStatSeedSize, cat.genetic_stats) ||
        !ReadStats(bytes, stat_start, kStatBonusOffset, cat.heredity_bonus) ||
        !ReadStats(bytes, stat_start, kStatEquipmentOffset, cat.equipment_bonus)) {
        return {{}, ErrorCode::CatDataUnavailable, "cat stats are truncated"};
    }
    cursor += kStatBlockSize;
    if (!ReadAsciiString(bytes, cursor, cat.stat_type_id)) {
        return {{}, ErrorCode::CatDataUnavailable, "cat stat type is invalid"};
    }
    if (kPreAbilityMetadataSize > bytes.size() - cursor) {
        return {
            {},
            ErrorCode::CatDataUnavailable,
            "cat pre-ability metadata is truncated"
        };
    }
    cursor += kPreAbilityMetadataSize;

    cat.raw_ability_slots.reserve(kAbilitySlotCount);
    for (std::size_t index = 0; index < kAbilitySlotCount; ++index) {
        std::string ability;
        if (!ReadAsciiString(bytes, cursor, ability, true)) {
            return {
                {},
                ErrorCode::CatDataUnavailable,
                "cat ability slot " + std::to_string(index + 1) +
                    " is invalid"
            };
        }
        cat.raw_ability_slots.push_back(std::move(ability));
    }
    (void)current_day;
    return {std::move(cat)};
}

}  // namespace autocattery::snapshot
