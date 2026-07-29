#include "auto_cattery/snapshot/save_snapshot_adapter.hpp"

#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

template<class T>
void Append(std::vector<std::uint8_t>& bytes, const T& value) {
    const auto* begin = reinterpret_cast<const std::uint8_t*>(&value);
    bytes.insert(bytes.end(), begin, begin + sizeof(T));
}

void AppendEntry(
    std::vector<std::uint8_t>& bytes,
    std::int64_t cat_id,
    std::string_view room) {
    Append(bytes, cat_id);
    Append(bytes, static_cast<std::uint64_t>(room.size()));
    bytes.insert(bytes.end(), room.begin(), room.end());
    Append(bytes, 1.0);
    Append(bytes, 2.0);
    Append(bytes, 3.0);
}

}  // namespace

void RunHouseStateParserTests() {
    std::vector<std::uint8_t> blob;
    Append(blob, std::uint32_t{0});
    Append(blob, std::uint32_t{2});
    AppendEntry(blob, 11, "Floor1_Large");
    AppendEntry(blob, 12, "AdventureBox");

    const auto parsed = snapshot::ParseHouseState(blob);
    AC_CHECK(static_cast<bool>(parsed));
    AC_CHECK(parsed.value.size() == 2);
    AC_CHECK(parsed.value[0].cat_id == 11);
    AC_CHECK(parsed.value[0].room_id == "Floor1_Large");
    AC_CHECK(parsed.value[1].room_id == "AdventureBox");

    auto trailing = blob;
    trailing.push_back(0);
    AC_CHECK(!static_cast<bool>(snapshot::ParseHouseState(trailing)));

    std::vector<std::uint8_t> invalid_header;
    Append(invalid_header, std::uint32_t{1});
    Append(invalid_header, std::uint32_t{0});
    AC_CHECK(!static_cast<bool>(
        snapshot::ParseHouseState(invalid_header)));
}

}  // namespace autocattery::tests
