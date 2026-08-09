#include "auto_cattery/snapshot/detail/furniture_geometry.hpp"

#include "test_support.hpp"

#include <windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace autocattery::tests {
namespace {

template<class T>
void Append(std::vector<std::byte>& bytes, T value) {
    const auto offset = bytes.size();
    bytes.resize(offset + sizeof(value));
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

void AppendText(std::vector<std::byte>& bytes, std::string_view text) {
    for (const char value : text) {
        bytes.push_back(static_cast<std::byte>(value));
    }
}

std::filesystem::path TemporaryPath() {
    wchar_t directory[MAX_PATH]{};
    wchar_t file[MAX_PATH]{};
    GetTempPathW(MAX_PATH, directory);
    GetTempFileNameW(directory, L"acg", 0, file);
    return std::filesystem::path(file);
}

std::filesystem::path WriteGpak(
    const std::vector<std::pair<std::string, std::vector<std::byte>>>& entries) {
    const auto path = TemporaryPath();
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    const auto count = static_cast<std::uint32_t>(entries.size());
    stream.write(reinterpret_cast<const char*>(&count), sizeof(count));
    for (const auto& [name, bytes] : entries) {
        const auto name_size = static_cast<std::uint16_t>(name.size());
        const auto data_size = static_cast<std::uint32_t>(bytes.size());
        stream.write(
            reinterpret_cast<const char*>(&name_size), sizeof(name_size));
        stream.write(name.data(), name_size);
        stream.write(
            reinterpret_cast<const char*>(&data_size), sizeof(data_size));
    }
    for (const auto& [name, bytes] : entries) {
        (void)name;
        stream.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    }
    return path;
}

std::vector<std::byte> FurnitureInfo(
    bool truncate_payload = false,
    bool trailing_byte = false) {
    std::vector<std::byte> bytes;
    Append<std::uint32_t>(bytes, 1);
    Append<std::uint32_t>(bytes, 2);
    for (const std::string_view item : {"chair", "table"}) {
        Append<std::uint32_t>(
            bytes, static_cast<std::uint32_t>(item.size()));
        Append<std::uint32_t>(bytes, item == "chair" ? 0U : 3U);
        AppendText(bytes, item);
        const auto payload_size =
            snapshot::detail::kFurnitureInfoOpaquePayloadSize;
        for (std::size_t index = 0; index < payload_size; ++index) {
            bytes.push_back(static_cast<std::byte>(index % 251U));
        }
    }
    if (truncate_payload) {
        bytes.pop_back();
    }
    if (trailing_byte) {
        bytes.push_back(std::byte{0});
    }
    return bytes;
}

std::vector<std::byte> HouseGon() {
    constexpr std::string_view text = R"(
rooms {
 R1 { width 16 height 7 }
 R2 { width 16 height 7 }
 R3 { width 16 height 7 }
 R4 { width 16 height 7 }
 R5 { width 16 height 7 }
 R6 { width 16 height 7 }
 Roof { id Attic width 18 height 5
   built_in_collision [[6 6 6 6] [6 0 0 6] [6 2 2 6]]
 }
}
houses {
 H1 { room_positions { R1 [0 0] } }
 H2 { room_positions { R1 [0 0] R2 [1, 0] } }
 H3 { room_positions { R1 [0 0] R2 [1 0] R3 [2 0] } }
 H5 { room_positions {
   R1 [0 0] R2 [1 0] R3 [2 0] R4 [3 0] R5 [4 0]
 } }
 H7 { room_positions {
   R1 [0 0] R2 [1 0] R3 [2 0] R4 [3 0] R5 [4 0]
   R6 [5 0] Roof [6 0]
 } }
}
)";
    std::vector<std::byte> bytes;
    AppendText(bytes, text);
    return bytes;
}

}  // namespace

void RunFurnitureGeometryTests() {
    std::string error;
    const auto geometry_path = WriteGpak({
        {"data/ignored.bin", {std::byte{1}, std::byte{2}}},
        {"data/house.gon", HouseGon()},
        {"data/furniture_info.data", FurnitureInfo()}});

    snapshot::detail::HouseGeometryCatalog geometry;
    AC_CHECK(snapshot::detail::LoadHouseGeometryCatalog(
        geometry_path, geometry, error));
    AC_CHECK(geometry.rooms.size() == 7);
    AC_CHECK(geometry.houses.size() == 5);
    const std::array<std::size_t, 5> expected_counts{1, 2, 3, 5, 7};
    for (std::size_t index = 0; index < expected_counts.size(); ++index) {
        AC_CHECK(
            geometry.houses[index].room_positions.size() ==
            expected_counts[index]);
    }
    AC_CHECK(geometry.rooms.back().definition_id == "Roof");
    AC_CHECK(geometry.rooms.back().room_id == "Attic");
    AC_CHECK(geometry.rooms.back().built_in_collision.size() == 3);
    AC_CHECK(geometry.rooms.back().built_in_collision.front().size() == 4);
    AC_CHECK(geometry.houses.back().room_positions.back().room_id == "Attic");

    snapshot::detail::FurnitureInfoCatalog furniture;
    AC_CHECK(snapshot::detail::LoadFurnitureInfoCatalog(
        geometry_path, furniture, error));
    AC_CHECK(furniture.format_version == 1);
    AC_CHECK(furniture.records.size() == 2);
    AC_CHECK(furniture.records[0].item_id == "chair");
    AC_CHECK(furniture.records[1].item_id == "table");
    AC_CHECK(furniture.records[0].unknown_after_name_length == 0);
    AC_CHECK(furniture.records[1].unknown_after_name_length == 3);
    AC_CHECK(furniture.records[0].opaque_payload.size() == 580);
    std::filesystem::remove(geometry_path);

    const auto truncated_path = WriteGpak({
        {"data/house.gon", HouseGon()},
        {"data/furniture_info.data", FurnitureInfo(true, false)}});
    AC_CHECK(!snapshot::detail::LoadFurnitureInfoCatalog(
        truncated_path, furniture, error));
    std::filesystem::remove(truncated_path);

    const auto trailing_path = WriteGpak({
        {"data/house.gon", HouseGon()},
        {"data/furniture_info.data", FurnitureInfo(false, true)}});
    AC_CHECK(!snapshot::detail::LoadFurnitureInfoCatalog(
        trailing_path, furniture, error));
    std::filesystem::remove(trailing_path);
}

}  // namespace autocattery::tests
