#include "auto_cattery/snapshot/detail/visual_traits.hpp"

#include <cctype>
#include <cstdint>
#include <fstream>
#include <string_view>
#include <vector>

namespace autocattery::snapshot::detail {
namespace {

template<class T>
bool Read(std::ifstream& stream, T& value) {
    return static_cast<bool>(stream.read(
        reinterpret_cast<char*>(&value), sizeof(value)));
}

std::string Key(std::string_view category, std::uint32_t id) {
    return std::string(category) + ":" + std::to_string(id);
}

void ParseMutationGon(
    std::string_view category,
    std::string_view text,
    MutationCatalog& catalog) {
    std::size_t cursor{};
    while (cursor < text.size()) {
        while (cursor < text.size() &&
               !std::isdigit(static_cast<unsigned char>(text[cursor])) &&
               text[cursor] != '-') {
            ++cursor;
        }
        const auto id_start = cursor;
        if (cursor < text.size() && text[cursor] == '-') {
            ++cursor;
        }
        while (cursor < text.size() &&
               std::isdigit(static_cast<unsigned char>(text[cursor]))) {
            ++cursor;
        }
        auto after_id = cursor;
        while (after_id < text.size() &&
               std::isspace(static_cast<unsigned char>(text[after_id]))) {
            ++after_id;
        }
        if (id_start == cursor || after_id >= text.size() ||
            text[after_id] != '{') {
            cursor = id_start + 1;
            continue;
        }
        int depth = 1;
        auto block_end = after_id + 1;
        while (block_end < text.size() && depth > 0) {
            depth += text[block_end] == '{' ? 1 : 0;
            depth -= text[block_end] == '}' ? 1 : 0;
            ++block_end;
        }
        if (depth != 0) {
            break;
        }
        const auto raw_id = text.substr(id_start, cursor - id_start);
        std::int64_t signed_id{};
        try {
            signed_id = std::stoll(std::string(raw_id));
        } catch (...) {
            cursor = block_end;
            continue;
        }
        const auto block = text.substr(
            after_id + 1, block_end - after_id - 2);
        const bool defect = block.find("tag birth_defect") !=
            std::string_view::npos;
        if (signed_id >= 300 || signed_id == -2 || defect) {
            const auto id = signed_id == -2
                ? std::uint32_t{0xFFFFFFFEU}
                : static_cast<std::uint32_t>(signed_id);
            catalog[Key(category, id)] = {
                defect || signed_id == -2
                    ? VisualTraitKind::BirthDefect
                    : VisualTraitKind::Mutation
            };
        }
        cursor = block_end;
    }
}

}  // namespace

bool LoadMutationCatalog(
    const std::filesystem::path& gpak_path,
    MutationCatalog& catalog,
    std::string& error) {
    std::ifstream stream(gpak_path, std::ios::binary);
    std::uint32_t count{};
    if (!stream || !Read(stream, count) || count > 1'000'000U) {
        error = "resources.gpak directory is unavailable";
        return false;
    }
    struct Entry { std::string name; std::uint32_t size{}; };
    std::vector<Entry> entries;
    entries.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index) {
        std::uint16_t length{};
        Entry entry;
        if (!Read(stream, length) || length > 1'000U) {
            error = "resources.gpak directory is invalid";
            return false;
        }
        entry.name.resize(length);
        if (!stream.read(entry.name.data(), length) ||
            !Read(stream, entry.size)) {
            error = "resources.gpak directory is truncated";
            return false;
        }
        entries.push_back(std::move(entry));
    }
    std::uint64_t offset = static_cast<std::uint64_t>(stream.tellg());
    catalog.clear();
    for (const auto& entry : entries) {
        const bool mutation_file =
            entry.name.starts_with("data/mutations/") &&
            entry.name.ends_with(".gon");
        if (mutation_file) {
            stream.seekg(static_cast<std::streamoff>(offset));
            std::string text(entry.size, '\0');
            if (!stream.read(text.data(), entry.size)) {
                error = entry.name + " is truncated";
                catalog.clear();
                return false;
            }
            const auto slash = entry.name.find_last_of('/');
            ParseMutationGon(
                std::string_view(entry.name).substr(
                    slash + 1, entry.name.size() - slash - 5),
                text,
                catalog);
        }
        offset += entry.size;
    }
    if (catalog.empty()) {
        error = "mutation GON catalog is missing";
        return false;
    }
    return true;
}

void ApplyMutationCatalog(
    CatSnapshot& cat,
    const MutationCatalog& catalog) {
    cat.visual_traits.clear();
    for (const auto& slot : cat.raw_visual_part_slots) {
        const auto found = catalog.find(Key(slot.category, slot.id));
        if (found == catalog.end()) {
            continue;
        }
        cat.visual_traits.push_back({
            slot.slot, slot.category, slot.id, found->second.kind
        });
    }
}

}  // namespace autocattery::snapshot::detail
