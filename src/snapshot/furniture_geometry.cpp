#include "auto_cattery/snapshot/detail/furniture_geometry.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <optional>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>

namespace autocattery::snapshot::detail {
namespace {

constexpr std::uint32_t kMaximumGpakEntries = 1'000'000U;
constexpr std::uint32_t kMaximumFurnitureInfoRecords = 1'000'000U;
constexpr std::size_t kMaximumRoomCollisionCells = 1'000'000U;

template<class T>
bool ReadStream(std::ifstream& stream, T& value) {
    return static_cast<bool>(stream.read(
        reinterpret_cast<char*>(&value), sizeof(value)));
}

bool ReadGpakEntry(
    const std::filesystem::path& gpak_path,
    std::string_view wanted_name,
    std::vector<std::byte>& bytes,
    std::string& error) {
    std::ifstream stream(gpak_path, std::ios::binary);
    std::uint32_t count{};
    if (!stream || !ReadStream(stream, count) ||
        count > kMaximumGpakEntries) {
        error = "resources.gpak directory is unavailable";
        return false;
    }

    struct Entry {
        std::string name;
        std::uint32_t size{};
    };
    std::vector<Entry> entries;
    entries.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index) {
        std::uint16_t length{};
        if (!ReadStream(stream, length)) {
            error = "resources.gpak directory is truncated";
            return false;
        }
        Entry entry;
        entry.name.resize(length);
        if ((length != 0U &&
             !stream.read(entry.name.data(), length)) ||
            !ReadStream(stream, entry.size)) {
            error = "resources.gpak directory is truncated";
            return false;
        }
        entries.push_back(std::move(entry));
    }

    const auto data_start = stream.tellg();
    if (data_start < 0) {
        error = "resources.gpak directory offset is invalid";
        return false;
    }
    std::uint64_t offset = static_cast<std::uint64_t>(data_start);
    std::error_code file_size_error;
    const auto file_size =
        std::filesystem::file_size(gpak_path, file_size_error);
    if (file_size_error) {
        error = "resources.gpak size is unavailable";
        return false;
    }
    for (const auto& entry : entries) {
        if (offset > file_size || entry.size > file_size - offset) {
            error = "resources.gpak entry range is invalid";
            return false;
        }
        if (entry.name == wanted_name) {
            stream.seekg(static_cast<std::streamoff>(offset));
            bytes.resize(entry.size);
            if (entry.size != 0U && !stream.read(
                    reinterpret_cast<char*>(bytes.data()), entry.size)) {
                error = std::string(wanted_name) + " is truncated";
                return false;
            }
            return true;
        }
        offset += entry.size;
    }
    error = std::string(wanted_name) + " is missing";
    return false;
}

enum class TokenKind {
    Word,
    LeftBrace,
    RightBrace,
    LeftBracket,
    RightBracket,
    Comma
};

struct Token {
    TokenKind kind{};
    std::string text;
};

std::vector<Token> Tokenize(std::string_view text) {
    std::vector<Token> tokens;
    std::size_t cursor{};
    while (cursor < text.size()) {
        const char current = text[cursor];
        if (current == '/' && cursor + 1U < text.size() &&
            text[cursor + 1U] == '/') {
            cursor += 2U;
            while (cursor < text.size() && text[cursor] != '\n') {
                ++cursor;
            }
            continue;
        }
        if (current == ' ' || current == '\t' || current == '\r' ||
            current == '\n') {
            ++cursor;
            continue;
        }
        const auto add_symbol = [&](TokenKind kind) {
            tokens.push_back({.kind = kind});
            ++cursor;
        };
        if (current == '{') {
            add_symbol(TokenKind::LeftBrace);
            continue;
        }
        if (current == '}') {
            add_symbol(TokenKind::RightBrace);
            continue;
        }
        if (current == '[') {
            add_symbol(TokenKind::LeftBracket);
            continue;
        }
        if (current == ']') {
            add_symbol(TokenKind::RightBracket);
            continue;
        }
        if (current == ',') {
            add_symbol(TokenKind::Comma);
            continue;
        }
        const auto start = cursor;
        while (cursor < text.size()) {
            const char value = text[cursor];
            if (value == ' ' || value == '\t' || value == '\r' ||
                value == '\n' || value == '{' || value == '}' ||
                value == '[' || value == ']' || value == ',') {
                break;
            }
            if (value == '/' && cursor + 1U < text.size() &&
                text[cursor + 1U] == '/') {
                break;
            }
            ++cursor;
        }
        tokens.push_back({
            .kind = TokenKind::Word,
            .text = std::string(text.substr(start, cursor - start))});
    }
    return tokens;
}

std::optional<std::size_t> MatchingToken(
    const std::vector<Token>& tokens,
    std::size_t open,
    TokenKind left,
    TokenKind right) {
    if (open >= tokens.size() || tokens[open].kind != left) {
        return std::nullopt;
    }
    std::size_t depth{};
    for (std::size_t index = open; index < tokens.size(); ++index) {
        if (tokens[index].kind == left) {
            ++depth;
        } else if (tokens[index].kind == right) {
            if (depth == 0U) {
                return std::nullopt;
            }
            --depth;
            if (depth == 0U) {
                return index;
            }
        }
    }
    return std::nullopt;
}

std::optional<std::pair<std::size_t, std::size_t>> NamedBlock(
    const std::vector<Token>& tokens,
    std::string_view name) {
    for (std::size_t index = 0; index + 1U < tokens.size(); ++index) {
        if (tokens[index].kind != TokenKind::Word ||
            tokens[index].text != name ||
            tokens[index + 1U].kind != TokenKind::LeftBrace) {
            continue;
        }
        const auto end = MatchingToken(
            tokens, index + 1U,
            TokenKind::LeftBrace, TokenKind::RightBrace);
        if (end) {
            return std::pair{index + 2U, *end};
        }
        return std::nullopt;
    }
    return std::nullopt;
}

bool ParseInteger(std::string_view text, std::int32_t& value) {
    const auto* begin = text.data();
    const auto* end = begin + text.size();
    const auto result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

bool ParseDouble(std::string_view text, double& value) {
    std::string owned(text);
    char* end{};
    value = std::strtod(owned.c_str(), &end);
    return end != owned.c_str() && *end == '\0';
}

bool ParseCollision(
    const std::vector<Token>& tokens,
    std::size_t open,
    std::size_t close,
    std::vector<std::vector<std::int32_t>>& collision) {
    collision.clear();
    std::size_t index = open + 1U;
    while (index < close) {
        if (tokens[index].kind != TokenKind::LeftBracket) {
            ++index;
            continue;
        }
        const auto row_end = MatchingToken(
            tokens, index, TokenKind::LeftBracket, TokenKind::RightBracket);
        if (!row_end || *row_end > close) {
            return false;
        }
        std::vector<std::int32_t> row;
        for (std::size_t cell = index + 1U; cell < *row_end; ++cell) {
            if (tokens[cell].kind == TokenKind::Comma) {
                continue;
            }
            std::int32_t value{};
            if (tokens[cell].kind != TokenKind::Word ||
                !ParseInteger(tokens[cell].text, value)) {
                return false;
            }
            row.push_back(value);
        }
        if (row.empty() ||
            (!collision.empty() && row.size() != collision.front().size())) {
            return false;
        }
        collision.push_back(std::move(row));
        index = *row_end + 1U;
    }
    return !collision.empty();
}

bool ParseRoomDefinition(
    const std::vector<Token>& tokens,
    std::string_view definition_id,
    std::size_t begin,
    std::size_t end,
    RoomGeometryDefinition& room) {
    room.definition_id = definition_id;
    room.room_id = std::string(definition_id);
    std::size_t index = begin;
    while (index < end) {
        if (tokens[index].kind != TokenKind::Word || index + 1U >= end) {
            ++index;
            continue;
        }
        const auto& key = tokens[index].text;
        const auto& value = tokens[index + 1U];
        if (value.kind == TokenKind::LeftBrace) {
            const auto close = MatchingToken(
                tokens, index + 1U,
                TokenKind::LeftBrace, TokenKind::RightBrace);
            if (!close || *close > end) {
                return false;
            }
            index = *close + 1U;
            continue;
        }
        if (value.kind == TokenKind::LeftBracket) {
            const auto close = MatchingToken(
                tokens, index + 1U,
                TokenKind::LeftBracket, TokenKind::RightBracket);
            if (!close || *close > end) {
                return false;
            }
            if (key == "built_in_collision" &&
                !ParseCollision(
                    tokens, index + 1U, *close,
                    room.built_in_collision)) {
                return false;
            }
            index = *close + 1U;
            continue;
        }
        if (value.kind != TokenKind::Word) {
            return false;
        }
        if (key == "id") {
            room.room_id = value.text;
        } else if (key == "width") {
            if (!ParseInteger(value.text, room.width)) {
                return false;
            }
        } else if (key == "height") {
            if (!ParseInteger(value.text, room.height)) {
                return false;
            }
        }
        index += 2U;
    }
    return !room.definition_id.empty() && !room.room_id.empty() &&
        room.width > 0 && room.height > 0;
}

bool ParseRooms(
    const std::vector<Token>& tokens,
    std::size_t begin,
    std::size_t end,
    HouseGeometryCatalog& catalog,
    std::string& error) {
    std::unordered_set<std::string> definitions;
    std::size_t index = begin;
    while (index < end) {
        if (tokens[index].kind != TokenKind::Word ||
            index + 1U >= end ||
            tokens[index + 1U].kind != TokenKind::LeftBrace) {
            error = "house.gon rooms block is invalid";
            return false;
        }
        const auto close = MatchingToken(
            tokens, index + 1U,
            TokenKind::LeftBrace, TokenKind::RightBrace);
        if (!close || *close > end) {
            error = "house.gon room definition is truncated";
            return false;
        }
        RoomGeometryDefinition room;
        if (!definitions.insert(tokens[index].text).second ||
            !ParseRoomDefinition(
                tokens, tokens[index].text,
                index + 2U, *close, room)) {
            error = "house.gon room definition is invalid";
            return false;
        }
        catalog.rooms.push_back(std::move(room));
        index = *close + 1U;
    }
    return !catalog.rooms.empty();
}

bool ParseRoomPositions(
    const std::vector<Token>& tokens,
    std::size_t begin,
    std::size_t end,
    const std::unordered_map<std::string, RoomId>& room_ids,
    std::vector<HouseRoomPosition>& positions) {
    std::size_t index = begin;
    while (index < end) {
        if (tokens[index].kind != TokenKind::Word ||
            index + 1U >= end ||
            tokens[index + 1U].kind != TokenKind::LeftBracket) {
            return false;
        }
        const auto definition = room_ids.find(tokens[index].text);
        const auto close = MatchingToken(
            tokens, index + 1U,
            TokenKind::LeftBracket, TokenKind::RightBracket);
        if (definition == room_ids.end() || !close || *close > end) {
            return false;
        }
        std::vector<double> coordinates;
        for (std::size_t value = index + 2U; value < *close; ++value) {
            if (tokens[value].kind == TokenKind::Comma) {
                continue;
            }
            double coordinate{};
            if (tokens[value].kind != TokenKind::Word ||
                !ParseDouble(tokens[value].text, coordinate)) {
                return false;
            }
            coordinates.push_back(coordinate);
        }
        if (coordinates.size() != 2U) {
            return false;
        }
        positions.push_back({
            .room_definition_id = tokens[index].text,
            .room_id = definition->second,
            .x = coordinates[0],
            .y = coordinates[1]});
        index = *close + 1U;
    }
    return !positions.empty();
}

bool ParseHouseDefinition(
    const std::vector<Token>& tokens,
    std::string_view house_id,
    std::size_t begin,
    std::size_t end,
    const std::unordered_map<std::string, RoomId>& room_ids,
    HouseLayoutDefinition& house) {
    house.house_id = house_id;
    std::size_t index = begin;
    while (index < end) {
        if (tokens[index].kind != TokenKind::Word || index + 1U >= end) {
            ++index;
            continue;
        }
        const auto& key = tokens[index].text;
        const auto& value = tokens[index + 1U];
        if (value.kind == TokenKind::LeftBrace) {
            const auto close = MatchingToken(
                tokens, index + 1U,
                TokenKind::LeftBrace, TokenKind::RightBrace);
            if (!close || *close > end) {
                return false;
            }
            if (key == "room_positions" && !ParseRoomPositions(
                    tokens, index + 2U, *close,
                    room_ids, house.room_positions)) {
                return false;
            }
            index = *close + 1U;
            continue;
        }
        if (value.kind == TokenKind::LeftBracket) {
            const auto close = MatchingToken(
                tokens, index + 1U,
                TokenKind::LeftBracket, TokenKind::RightBracket);
            if (!close || *close > end) {
                return false;
            }
            index = *close + 1U;
            continue;
        }
        index += 2U;
    }
    return !house.house_id.empty() && !house.room_positions.empty();
}

bool ParseHouses(
    const std::vector<Token>& tokens,
    std::size_t begin,
    std::size_t end,
    HouseGeometryCatalog& catalog,
    std::string& error) {
    std::unordered_map<std::string, RoomId> room_ids;
    for (const auto& room : catalog.rooms) {
        room_ids.emplace(room.definition_id, room.room_id);
    }
    std::unordered_set<std::string> house_ids;
    std::size_t index = begin;
    while (index < end) {
        if (tokens[index].kind != TokenKind::Word ||
            index + 1U >= end ||
            tokens[index + 1U].kind != TokenKind::LeftBrace) {
            error = "house.gon houses block is invalid";
            return false;
        }
        const auto close = MatchingToken(
            tokens, index + 1U,
            TokenKind::LeftBrace, TokenKind::RightBrace);
        if (!close || *close > end) {
            error = "house.gon house definition is truncated";
            return false;
        }
        HouseLayoutDefinition house;
        if (!house_ids.insert(tokens[index].text).second ||
            !ParseHouseDefinition(
                tokens, tokens[index].text,
                index + 2U, *close, room_ids, house)) {
            error = "house.gon house definition is invalid";
            return false;
        }
        catalog.houses.push_back(std::move(house));
        index = *close + 1U;
    }
    return !catalog.houses.empty();
}

bool ParseHouseGon(
    std::string_view text,
    HouseGeometryCatalog& catalog,
    std::string& error) {
    const auto tokens = Tokenize(text);
    const auto rooms = NamedBlock(tokens, "rooms");
    const auto houses = NamedBlock(tokens, "houses");
    if (!rooms || !houses) {
        error = "house.gon required blocks are missing";
        return false;
    }
    HouseGeometryCatalog parsed;
    if (!ParseRooms(tokens, rooms->first, rooms->second, parsed, error) ||
        !ParseHouses(tokens, houses->first, houses->second, parsed, error)) {
        return false;
    }
    catalog = std::move(parsed);
    return true;
}

template<class T>
bool ReadBytes(
    const std::vector<std::byte>& bytes,
    std::size_t& offset,
    T& value) {
    if (offset > bytes.size() || bytes.size() - offset < sizeof(T)) {
        return false;
    }
    std::memcpy(&value, bytes.data() + offset, sizeof(T));
    offset += sizeof(T);
    return true;
}

bool ParseFurnitureInfo(
    const std::vector<std::byte>& bytes,
    FurnitureInfoCatalog& catalog,
    std::string& error) {
    std::size_t offset{};
    std::uint32_t count{};
    FurnitureInfoCatalog parsed;
    if (!ReadBytes(bytes, offset, parsed.format_version) ||
        parsed.format_version != 1U ||
        !ReadBytes(bytes, offset, count) ||
        count > kMaximumFurnitureInfoRecords) {
        error = "furniture_info.data header is invalid";
        return false;
    }
    parsed.records.reserve(count);
    std::unordered_set<std::string> item_ids;
    for (std::uint32_t index = 0; index < count; ++index) {
        std::uint32_t name_length{};
        FurnitureInfoRecord record;
        if (!ReadBytes(bytes, offset, name_length) ||
            !ReadBytes(
                bytes, offset,
                record.unknown_after_name_length) ||
            name_length == 0U ||
            name_length > bytes.size() - offset) {
            error = "furniture_info.data record name is invalid";
            return false;
        }
        record.item_id.assign(
            reinterpret_cast<const char*>(bytes.data() + offset),
            static_cast<std::size_t>(name_length));
        offset += static_cast<std::size_t>(name_length);
        if (!item_ids.insert(record.item_id).second ||
            offset > bytes.size() ||
            bytes.size() - offset < record.opaque_payload.size()) {
            error = "furniture_info.data record payload is invalid";
            return false;
        }
        std::copy_n(
            bytes.data() + offset,
            record.opaque_payload.size(),
            record.opaque_payload.begin());
        offset += record.opaque_payload.size();

        record.placement_grid.supported = true;
        for (std::size_t tile_index = 0;
             tile_index < kFurniturePlacementGridCellCount;
             ++tile_index) {
            const auto raw = std::to_integer<std::uint8_t>(
                record.opaque_payload[
                    kFurniturePlacementGridOffset + tile_index]);
            if (raw > static_cast<std::uint8_t>(
                    FurniturePlacementTile::PoopLogic)) {
                record.placement_grid.supported = false;
                break;
            }
            record.placement_grid.tiles[tile_index] =
                static_cast<FurniturePlacementTile>(raw);
        }
        for (std::size_t payload_index = 0;
             payload_index < record.opaque_payload.size();
             ++payload_index) {
            const bool inside_grid =
                payload_index >= kFurniturePlacementGridOffset &&
                payload_index < kFurniturePlacementGridOffset +
                    kFurniturePlacementGridCellCount;
            if (!inside_grid && record.opaque_payload[payload_index] !=
                    std::byte{0}) {
                ++record.nonzero_bytes_outside_placement_grid;
            }
        }
        parsed.records.push_back(std::move(record));
    }
    if (offset != bytes.size()) {
        error = "furniture_info.data has trailing bytes";
        return false;
    }
    catalog = std::move(parsed);
    return true;
}

}  // namespace

RoomCollisionGrid DecodeRoomCollisionGrid(
    const RoomGeometryDefinition& room) {
    RoomCollisionGrid decoded;
    if (room.width <= 0 || room.height <= 0) {
        return decoded;
    }
    const auto width = static_cast<std::int64_t>(room.width) + 2;
    const auto height = static_cast<std::int64_t>(room.height) + 2;
    if (width <= 0 || height <= 0) {
        return decoded;
    }
    decoded.width = static_cast<std::size_t>(width);
    decoded.height = static_cast<std::size_t>(height);
    if (decoded.width > kMaximumRoomCollisionCells / decoded.height) {
        return {};
    }
    decoded.cells.assign(decoded.width * decoded.height, 0U);

    if (room.built_in_collision.empty()) {
        for (std::size_t x = 0; x < decoded.width; ++x) {
            decoded.cells[x] = 2U;
            decoded.cells[(decoded.height - 1U) * decoded.width + x] = 2U;
        }
        for (std::size_t y = 0; y < decoded.height; ++y) {
            decoded.cells[y * decoded.width] = 2U;
            decoded.cells[y * decoded.width + decoded.width - 1U] = 2U;
        }
        decoded.supported = true;
        return decoded;
    }

    if (room.built_in_collision.size() != decoded.height) {
        return {};
    }
    for (std::size_t y = 0; y < decoded.height; ++y) {
        const auto& source =
            room.built_in_collision[decoded.height - y - 1U];
        if (source.size() != decoded.width) {
            return {};
        }
        for (std::size_t x = 0; x < decoded.width; ++x) {
            const auto value = source[x];
            if (value < 0 || value > 255) {
                return {};
            }
            decoded.cells[y * decoded.width + x] =
                static_cast<std::uint8_t>(value);
        }
    }
    decoded.supported = true;
    return decoded;
}

bool LoadHouseGeometryCatalog(
    const std::filesystem::path& gpak_path,
    HouseGeometryCatalog& catalog,
    std::string& error) {
    std::vector<std::byte> bytes;
    if (!ReadGpakEntry(
            gpak_path, "data/house.gon", bytes, error)) {
        return false;
    }
    const std::string_view text(
        reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return ParseHouseGon(text, catalog, error);
}

bool LoadFurnitureInfoCatalog(
    const std::filesystem::path& gpak_path,
    FurnitureInfoCatalog& catalog,
    std::string& error) {
    std::vector<std::byte> bytes;
    if (!ReadGpakEntry(
            gpak_path, "data/furniture_info.data", bytes, error)) {
        return false;
    }
    return ParseFurnitureInfo(bytes, catalog, error);
}

}  // namespace autocattery::snapshot::detail
