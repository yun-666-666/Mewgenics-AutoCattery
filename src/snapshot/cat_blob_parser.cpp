#include "auto_cattery/snapshot/save_snapshot_adapter.hpp"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstring>

#include "auto_cattery/snapshot/detail/lz4_block.hpp"
#include "auto_cattery/snapshot/detail/cat_personality.hpp"
#include "auto_cattery/snapshot/detail/visual_traits.hpp"

namespace autocattery::snapshot {
namespace {

constexpr std::uint32_t kCatMagic = 19;
constexpr std::size_t kNameLengthOffset = 12;
constexpr std::size_t kNameStart = 20;
constexpr std::size_t kPostDescriptorMetadataSize = 16;
constexpr std::size_t kEquipmentBlockSize = 368;
constexpr std::size_t kStatBlockSize = 92;
constexpr std::size_t kStatSeedSize = 8;
constexpr std::size_t kStatBonusOffset = 36;
constexpr std::size_t kStatEquipmentOffset = 64;
constexpr std::size_t kPreAbilityMetadataSize = 14;
// Core save order verified against the current save format:
// move, basic attack, four active abilities, two passives, two disorders.
constexpr std::size_t kAbilitySlotCount = 10;
constexpr std::size_t kExtendedAbilitySlotCount = 4;
constexpr std::size_t kPostClassMetadataSize = 115;
constexpr std::size_t kPostClassBirthDayOffset = 12;
constexpr std::size_t kPostClassDeathDayOffset = 20;
// Format 19 stores an 8-byte byte-vector length after death_day, followed
// by that many bytes. The old-state int follows the vector payload.
constexpr std::size_t kPostClassOldStateOffset = 36;

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

bool ReadClassId(
    std::span<const std::uint8_t> bytes,
    std::size_t search_start,
    std::string& class_id, std::size_t& post_class_start,
    std::size_t& history_size) {
    if (bytes.size() < kPostClassMetadataSize + sizeof(std::uint64_t) ||
        search_start >= bytes.size() - kPostClassMetadataSize) {
        return false;
    }
    const auto search_end = bytes.size() - kPostClassMetadataSize;
    for (auto offset = search_start;
         offset + sizeof(std::uint64_t) <= search_end;
         ++offset) {
        auto cursor = offset;
        std::string candidate;
        std::uint64_t payload_size{};
        if (ReadAsciiString(bytes, cursor, candidate) &&
            cursor <= search_end && ReadAt(bytes, cursor + 28, payload_size) &&
            payload_size == search_end - cursor) {
            class_id = std::move(candidate);
            post_class_start = cursor;
            history_size = static_cast<std::size_t>(payload_size);
            return true;
        }
    }
    return false;
}

CatSex SexFromVoiceId(std::string voice_id) {
    std::ranges::transform(
        voice_id,
        voice_id.begin(),
        [](unsigned char value) {
            return static_cast<char>(std::tolower(value));
        });
    if (voice_id.starts_with("female")) {
        return CatSex::Female;
    }
    if (voice_id.starts_with("male")) {
        return CatSex::Male;
    }
    return CatSex::Unknown;
}

}  // namespace

Result<CatSnapshot> ParseCatBlob(
    CatId cat_id,
    std::span<const std::uint8_t> blob,
    std::optional<std::int64_t> current_day,
    bool base_stats_unlocked,
    bool sexuality_unlocked) {
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
    std::string stat_affinity;
    if (!ReadAsciiString(bytes, cursor, stat_affinity, true) ||
        kPostDescriptorMetadataSize > bytes.size() - cursor) {
        return {{}, ErrorCode::CatDataUnavailable, "cat metadata is truncated"};
    }
    cursor += kPostDescriptorMetadataSize;
    if (!ReadAsciiString(bytes, cursor, cat.breed_id) ||
        kEquipmentBlockSize > bytes.size() - cursor) {
        return {{}, ErrorCode::CatDataUnavailable, "cat breed block is invalid"};
    }
    if (sexuality_unlocked) {
        detail::ApplyUnlockedSexuality(bytes, cursor, cat);
    }
    detail::ParseVisualPartSlots(bytes, cursor, cat);
    cursor += kEquipmentBlockSize;
    if (!ReadAsciiString(bytes, cursor, cat.voice_id) ||
        kStatBlockSize > bytes.size() - cursor) {
        return {{}, ErrorCode::CatDataUnavailable, "cat voice/stat block is invalid"};
    }
    cat.sex = SexFromVoiceId(cat.voice_id);

    const auto stat_start = cursor;
    if (!ReadStats(bytes, stat_start, kStatSeedSize, cat.genetic_stats) ||
        !ReadStats(bytes, stat_start, kStatBonusOffset, cat.heredity_bonus) ||
        !ReadStats(bytes, stat_start, kStatEquipmentOffset, cat.equipment_bonus)) {
        return {{}, ErrorCode::CatDataUnavailable, "cat stats are truncated"};
    }
    if (!base_stats_unlocked) {
        cat.genetic_stats = {};
        cat.heredity_bonus = {};
        cat.equipment_bonus = {};
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
    for (std::size_t index = 0;
         index < kExtendedAbilitySlotCount;
         ++index) {
        std::string ability;
        std::uint32_t ability_level{};
        if (!ReadAsciiString(bytes, cursor, ability, true) ||
            !ReadAt(bytes, cursor, ability_level)) {
            return {
                {},
                ErrorCode::CatDataUnavailable,
                "cat extended ability slot " +
                    std::to_string(index + 1) + " is invalid"
            };
        }
        cursor += sizeof(ability_level);
    }
    std::size_t post_class_start{}, history_size{};
    if (!ReadClassId(bytes, cursor, cat.class_id, post_class_start, history_size)) {
        return {
            {},
            ErrorCode::CatDataUnavailable,
            "cat class is invalid"
        };
    }
    std::int64_t birth_day{};
    std::int64_t death_day{};
    std::int32_t old_state{};
    if (!ReadAt(
            bytes,
            post_class_start + kPostClassBirthDayOffset,
            birth_day) ||
        !ReadAt(
            bytes,
            post_class_start + kPostClassDeathDayOffset,
            death_day) ||
        !ReadAt(bytes, post_class_start + kPostClassOldStateOffset + history_size, old_state) ||
        birth_day < 0 || death_day < -1) {
        return {
            {},
            ErrorCode::CatDataUnavailable,
            "cat life-state block is invalid"
        };
    }
    cat.birth_day = birth_day;
    if (current_day && *current_day >= birth_day) {
        cat.age_days = *current_day - birth_day;
    }
    if (death_day >= 0) {
        cat.life_stage = LifeStage::Dead;
    } else if (old_state >= 2) {
        cat.life_stage = LifeStage::Senior;
    } else if (std::ranges::find(
                   cat.raw_ability_slots,
                   "EternalYouth") != cat.raw_ability_slots.end() ||
               (cat.age_days && *cat.age_days < 3)) {
        cat.life_stage = LifeStage::Kitten;
    } else if (cat.age_days) {
        cat.life_stage = LifeStage::Adult;
    }
    // Colorless is the save's not-yet-committed combat class. Death is an
    // independent persisted state and must always exclude the cat, including
    // a dead cat that still has the Colorless class.
    cat.available_for_combat =
        cat.life_stage != LifeStage::Dead && cat.class_id == "Colorless"
            ? TriState::Yes
            : TriState::No;
    cat.available_for_breeding =
        cat.life_stage == LifeStage::Adult
            ? TriState::Yes
            : cat.life_stage == LifeStage::Kitten ||
                    cat.life_stage == LifeStage::Senior ||
                    cat.life_stage == LifeStage::Dead
                ? TriState::No
                : TriState::Unknown;
    return {std::move(cat)};
}

}  // namespace autocattery::snapshot
