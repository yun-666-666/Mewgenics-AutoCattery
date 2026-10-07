#include "auto_cattery/snapshot/save_snapshot_adapter.hpp"
#include "auto_cattery/snapshot/detail/save_database.hpp"
#include "auto_cattery/snapshot/detail/pedigree_parser.hpp"
#include "auto_cattery/snapshot/detail/lz4_block.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <cstring>
#include <stdexcept>

namespace {
template<class T>
void Optional(std::ostream& out, const std::optional<T>& value) {
    if (value) out << *value;
    else out << "null";
}

template<class T>
T Read(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || sizeof(T) > bytes.size() - offset)
        throw std::runtime_error("research field is truncated");
    T value{};
    std::memcpy(&value, bytes.data() + offset, sizeof(T));
    return value;
}

std::string ReadString(std::span<const std::uint8_t> bytes, std::size_t& cursor) {
    const auto length = Read<std::uint64_t>(bytes, cursor);
    cursor += 8;
    if (cursor > bytes.size() || length > bytes.size() - cursor)
        throw std::runtime_error("research string is truncated");
    std::string result(reinterpret_cast<const char*>(bytes.data() + cursor), length);
    cursor += static_cast<std::size_t>(length);
    return result;
}

void ExportBreedingFields(std::ostream& out, std::span<const std::uint8_t> blob,
                         const autocattery::snapshot::CatSnapshot& cat) {
    using namespace autocattery::snapshot;
    const auto decoded = detail::DecodeCatStorageBlob(blob);
    if (!decoded) throw std::runtime_error(decoded.message);
    const auto bytes = std::span<const std::uint8_t>(decoded.value);
    auto cursor = 20 + 2 * static_cast<std::size_t>(Read<std::uint32_t>(bytes, 12));
    cursor += 8 + static_cast<std::size_t>(Read<std::uint64_t>(bytes, cursor));
    // Format 19 serializer RVA 22F64F: sex, presentation, flags, breed string.
    const auto sex = Read<std::int32_t>(bytes, cursor);
    const auto presentation = Read<std::int32_t>(bytes, cursor + 4);
    const auto flags = Read<std::uint64_t>(bytes, cursor + 8);
    cursor += 16;
    cursor += 8 + static_cast<std::size_t>(Read<std::uint64_t>(bytes, cursor));
    out << ",\"native_sex\":" << sex << ",\"sex_presentation\":" << presentation
        << ",\"no_breed\":" << ((flags & 0x200000) ? "true" : "false")
        << ",\"lover_id\":" << Read<std::int64_t>(bytes, cursor + 20)
        << ",\"love\":" << Read<double>(bytes, cursor + 28)
        << ",\"aggression\":" << Read<double>(bytes, cursor + 36)
        << ",\"enemy_id\":" << Read<std::int64_t>(bytes, cursor + 44)
        << ",\"hate\":" << Read<double>(bytes, cursor + 52)
        << ",\"fertility\":" << Read<double>(bytes, cursor + 60);
    // Reuse the parser's format-19 tail contract, including variable history.
    bool found = false;
    for (auto offset = cursor; offset + 8 + cat.class_id.size() + 115 <= bytes.size(); ++offset) {
        if (Read<std::uint64_t>(bytes, offset) != cat.class_id.size()) continue;
        if (std::memcmp(bytes.data() + offset + 8, cat.class_id.data(), cat.class_id.size())) continue;
        const auto tail = offset + 8 + cat.class_id.size();
        const auto history = Read<std::uint64_t>(bytes, tail + 28);
        if (history != bytes.size() - tail - 115) continue;
        out << ",\"old_state\":" << Read<std::int32_t>(bytes, tail + 36 + history);
        found = true;
        break;
    }
    if (!found) throw std::runtime_error("research old-state field not located");
    out << ",\"birth_day\":"; Optional(out, cat.birth_day);
    out << ",\"abilities\":[";
    for (std::size_t i = 0; i < cat.raw_ability_slots.size(); ++i) {
        if (i) out << ',';
        out << std::quoted(cat.raw_ability_slots[i]);
    }
    out << ']';
    // The ten unlevelled strings are native 7D0..8F0. The following four
    // levelled slots are native 910/938/960/988, including passives/disorders.
    auto ability_cursor = cursor + 368;
    ReadString(bytes, ability_cursor);  // voice
    ability_cursor += 92;
    ReadString(bytes, ability_cursor);  // stat type
    ability_cursor += 14;
    for (int i = 0; i < 10; ++i) ReadString(bytes, ability_cursor);
    out << ",\"passives\":[";
    for (int i = 0; i < 4; ++i) {
        const auto id = ReadString(bytes, ability_cursor);
        const auto level = Read<std::int32_t>(bytes, ability_cursor);
        ability_cursor += 4;
        if (i) out << ',';
        out << '[' << std::quoted(id) << ',' << level << ']';
    }
    out << "],\"class_id\":" << std::quoted(cat.class_id) << ",\"parts\":[";
    for (std::size_t i = 0; i < cat.raw_visual_part_slots.size(); ++i) {
        const auto& part = cat.raw_visual_part_slots[i];
        if (i) out << ',';
        out << '[' << std::quoted(part.slot) << ',' << std::quoted(part.category)
            << ',' << part.id << ']';
    }
    out << ']';
}
}

// Explicit offline research export: read persisted personality/pedigree even
// when its UI unlock is absent. This does not change production unlock gates.
int wmain(int argc, wchar_t** argv) {
    using namespace autocattery::snapshot;
    if (argc != 4) {
        std::cerr << "usage: breeding_snapshot_export SAVE GAME_ROOT OUTPUT.json\n";
        return 2;
    }
    SaveSnapshotAdapter adapter(argv[1], argv[2]);
    const auto snapshot = adapter.CaptureHouseSnapshot(1);
    if (!snapshot) { std::cerr << snapshot.message; return 1; }
    std::string error;
    auto db = detail::SaveDatabase::OpenReadOnly(argv[1], error);
    std::vector<detail::CatStorageRecord> records;
    std::optional<std::int32_t> day;
    std::optional<std::vector<std::byte>> pedigree_blob;
    if (!db || !db->ReadCats(records, error) || !db->ReadCurrentDay(day, error) ||
        !db->ReadFileBlob("pedigree", pedigree_blob, error) || !pedigree_blob) {
        std::cerr << "read failed: " << error; return 1;
    }
    const auto pedigree = detail::ParsePedigreeBlob(*pedigree_blob);
    if (!pedigree) { std::cerr << pedigree.message; return 1; }
    std::vector<detail::FurnitureStorageRecord> furniture_records;
    std::vector<detail::FurniturePlacement> placements;
    if (!db->ReadFurniture(furniture_records, error) ||
        !detail::ParseFurniturePlacements(furniture_records, placements, error)) {
        std::cerr << "furniture read failed: " << error; return 1;
    }
    std::ofstream out{std::filesystem::path(argv[3])};
    if (!out) return 1;
    out << std::setprecision(17) << "{\"day\":";
    Optional(out, day);
    out << ",\"offline_research_unlock_override\":true,\"cats\":[";
    bool first = true;
    for (const auto& resident : snapshot.value.cats) {
        const auto record = std::ranges::find(records, resident.id, &detail::CatStorageRecord::id);
        if (record == records.end()) return 1;
        const auto bytes = std::span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(record->blob.data()), record->blob.size());
        const auto parsed = ParseCatBlob(resident.id, bytes, day, true, true);
        if (!parsed) { std::cerr << parsed.message; return 1; }
        const auto& cat = parsed.value;
        if (!first) out << ',';
        first = false;
        out << "{\"id\":" << cat.id << ",\"sex\":" << static_cast<int>(cat.sex)
            << ",\"life_stage\":" << static_cast<int>(cat.life_stage) << ",\"age\":";
        Optional(out, cat.age_days);
        out << ",\"room_id\":";
        if (resident.room_id) out << std::quoted(*resident.room_id);
        else out << "null";
        out << ",\"in_adventure_box\":" << (resident.in_adventure_box ? "true" : "false");
        out << ",\"libido\":"; Optional(out, cat.libido_coefficient);
        out << ",\"sexuality\":"; Optional(out, cat.sexuality_coefficient);
        out << ",\"genetic\":[";
        for (std::size_t i = 0; i < kStatCount; ++i) {
            if (i) out << ',';
            Optional(out, cat.genetic_stats.values[i]);
        }
        out << ']';
        try {
            ExportBreedingFields(out, bytes, cat);
        } catch (const std::exception& exception) {
            std::cerr << exception.what(); return 1;
        }
        out << '}';
    }
    out << "],\"rooms\":[";
    first = true;
    for (const auto& room : snapshot.value.rooms) {
        if (!first) out << ',';
        first = false;
        out << "{\"id\":" << std::quoted(room.id) << ",\"attributes\":";
        if (room.attributes) {
            const auto& values = *room.attributes;
            out << "{\"Comfort\":" << values.comfort
                << ",\"Stimulation\":" << values.stimulation
                << ",\"Health\":" << values.health
                << ",\"Evolution\":" << values.mutation
                << ",\"Appeal\":" << values.appeal << '}';
        } else out << "null";
        out << '}';
    }
    out << "],\"placed_furniture\":[";
    first = true;
    for (const auto& placement : placements) {
        if (!first) out << ',';
        first = false;
        out << "{\"item_id\":" << std::quoted(placement.item_id)
            << ",\"room_id\":" << std::quoted(placement.room_id) << '}';
    }
    out << "],\"pedigree\":[";
    first = true;
    for (const auto& entry : pedigree.value.entries) {
        if (!first) out << ',';
        first = false;
        out << "[" << entry.cat_id << ',';
        Optional(out, entry.parent_a_id); out << ',';
        Optional(out, entry.parent_b_id); out << ',';
        Optional(out, entry.inbreeding_coefficient); out << ']';
    }
    out << "],\"pair_coi\":[";
    first = true;
    for (const auto& pair : pedigree.value.pair_coefficients) {
        if (!first) out << ',';
        first = false;
        out << '[' << pair.cat_a_id << ',' << pair.cat_b_id << ',' << pair.coefficient << ']';
    }
    out << "]}\n";
    return out ? 0 : 1;
}
