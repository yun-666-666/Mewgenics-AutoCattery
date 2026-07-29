#include "auto_cattery/snapshot/detail/lz4_block.hpp"

#include <cstdint>
#include <vector>

#include "test_support.hpp"

namespace autocattery::tests {

void RunLz4BlockTests() {
    const std::vector<std::uint8_t> raw{
        19, 0, 0, 0,
        1, 2, 3, 4
    };
    const auto raw_result =
        snapshot::detail::DecodeCatStorageBlob(raw);
    AC_CHECK(static_cast<bool>(raw_result));
    AC_CHECK(raw_result.value == raw);

    std::vector<std::uint8_t> literal_only{
        20, 0, 0, 0,
        0xF0, 5,
        19, 0, 0, 0,
        1, 2, 3, 4, 5, 6, 7, 8,
        9, 10, 11, 12, 13, 14, 15, 16
    };
    const auto literal_result =
        snapshot::detail::DecodeCatStorageBlob(literal_only);
    AC_CHECK(static_cast<bool>(literal_result));
    AC_CHECK(literal_result.value.size() == 20);
    AC_CHECK(literal_result.value.front() == 19);

    const std::vector<std::uint8_t> invalid_offset{
        20, 0, 0, 0,
        0x10, 19,
        0, 0
    };
    AC_CHECK(!static_cast<bool>(
        snapshot::detail::DecodeCatStorageBlob(invalid_offset)));
}

}  // namespace autocattery::tests
