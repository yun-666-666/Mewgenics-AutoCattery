#include "auto_cattery/snapshot/detail/furniture_attributes.hpp"

#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace autocattery::snapshot::detail {
namespace {

template<class T>
bool Read(std::ifstream& stream, T& value) {
    return static_cast<bool>(stream.read(
        reinterpret_cast<char*>(&value), sizeof(value)));
}

bool ReadName(
    std::ifstream& stream,
    std::uint16_t length,
    std::string& name) {
    name.resize(length);
    return length == 0 ||
        static_cast<bool>(stream.read(name.data(), length));
}

bool ParseNumber(std::string_view text, double& value) {
    std::string owned(text);
    char* end{};
    value = std::strtod(owned.c_str(), &end);
    return end != owned.c_str() && *end == '\0';
}

void SetEffect(
    RoomAttributes& effects,
    std::string_view key,
    double value) {
    if (key == "Comfort") {
        effects.comfort = value;
    } else if (key == "Stimulation") {
        effects.stimulation = value;
    } else if (key == "Health") {
        effects.health = value;
    } else if (key == "Evolution") {
        effects.mutation = value;
    } else if (key == "Appeal") {
        effects.appeal = value;
    }
}

void ParseBlock(
    std::string_view id,
    std::string_view block,
    FurnitureCatalog& catalog) {
    RoomAttributes effects;
    std::istringstream lines{std::string(block)};
    for (std::string line; std::getline(lines, line);) {
        if (const auto comment = line.find("//");
            comment != std::string::npos) {
            line.erase(comment);
        }
        std::istringstream fields(line);
        std::string key;
        std::string raw;
        if (!(fields >> key >> raw)) {
            continue;
        }
        double value{};
        if (ParseNumber(raw, value)) {
            SetEffect(effects, key, value);
        }
    }
    catalog.emplace(id, effects);
}

void ParseGon(std::string_view text, FurnitureCatalog& catalog) {
    std::size_t cursor{};
    while (cursor < text.size()) {
        const auto brace = text.find('{', cursor);
        if (brace == std::string_view::npos) {
            break;
        }
        auto id_end = brace;
        while (id_end > 0 &&
               std::isspace(static_cast<unsigned char>(text[id_end - 1]))) {
            --id_end;
        }
        auto id_start = id_end;
        while (id_start > 0) {
            const auto ch = static_cast<unsigned char>(text[id_start - 1]);
            if (!std::isalnum(ch) && ch != '_') {
                break;
            }
            --id_start;
        }
        if (id_start == id_end) {
            cursor = brace + 1;
            continue;
        }
        std::size_t end = brace + 1;
        int depth = 1;
        while (end < text.size() && depth > 0) {
            depth += text[end] == '{' ? 1 : 0;
            depth -= text[end] == '}' ? 1 : 0;
            ++end;
        }
        if (depth != 0) {
            break;
        }
        ParseBlock(
            text.substr(id_start, id_end - id_start),
            text.substr(brace + 1, end - brace - 2),
            catalog);
        cursor = end;
    }
}

}  // namespace

bool LoadFurnitureCatalog(
    const std::filesystem::path& gpak_path,
    FurnitureCatalog& catalog,
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
        if (!Read(stream, length) || !ReadName(stream, length, entry.name) ||
            !Read(stream, entry.size)) {
            error = "resources.gpak directory is truncated";
            return false;
        }
        entries.push_back(std::move(entry));
    }
    std::uint64_t offset = static_cast<std::uint64_t>(stream.tellg());
    for (const auto& entry : entries) {
        if (entry.name == "data/furniture_effects.gon") {
            stream.seekg(static_cast<std::streamoff>(offset));
            std::string text(entry.size, '\0');
            if (!stream.read(text.data(), entry.size)) {
                error = "furniture_effects.gon is truncated";
                return false;
            }
            catalog.clear();
            ParseGon(text, catalog);
            return !catalog.empty();
        }
        offset += entry.size;
    }
    error = "furniture_effects.gon is missing";
    return false;
}

}  // namespace autocattery::snapshot::detail
