#include "auto_cattery/snapshot/detail/pedigree_parser.hpp"
#include "auto_cattery/snapshot/detail/progress_unlocks.hpp"

#include <array>
#include <cstring>
#include <string_view>

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

template<class T>
void Append(std::vector<std::byte>& bytes, const T& value) {
    const auto* first = reinterpret_cast<const std::byte*>(&value);
    bytes.insert(bytes.end(), first, first + sizeof(value));
}

void AppendTable(
    std::vector<std::byte>& bytes,
    std::size_t row_size,
    std::span<const std::byte> row) {
    Append(bytes, std::uint64_t{0xfffffffffffffff5ULL});
    Append(bytes, std::uint64_t{1});
    Append(bytes, std::uint64_t{1});
    bytes.push_back(std::byte{0});
    bytes.resize(bytes.size() + 17, std::byte{0x80});
    bytes.insert(bytes.end(), row.begin(), row.end());
    AC_CHECK(row.size() == row_size);
    Append(bytes, std::uint64_t{0});
}

std::vector<std::byte> PedigreeBlob() {
    std::vector<std::byte> blob;
    std::vector<std::byte> pedigree;
    Append(pedigree, std::int64_t{42});
    Append(pedigree, std::int64_t{11});
    Append(pedigree, std::int64_t{12});
    Append(pedigree, double{0.125});
    AppendTable(blob, 32, pedigree);

    std::vector<std::byte> pair;
    Append(pair, std::int64_t{11});
    Append(pair, std::int64_t{12});
    Append(pair, double{0.25});
    AppendTable(blob, 24, pair);

    std::vector<std::byte> accessible;
    Append(accessible, std::int64_t{42});
    AppendTable(blob, 8, accessible);
    return blob;
}

std::vector<std::byte> Bytes(std::string_view text) {
    const auto* begin = reinterpret_cast<const std::byte*>(text.data());
    return {begin, begin + text.size()};
}

}  // namespace

void RunPedigreeParserTests() {
    const auto parsed = snapshot::detail::ParsePedigreeBlob(PedigreeBlob());
    AC_CHECK(static_cast<bool>(parsed));
    AC_CHECK(parsed.value.entries.size() == 1);
    AC_CHECK(parsed.value.entries[0].cat_id == 42);
    AC_CHECK(parsed.value.entries[0].parent_a_id == 11);
    AC_CHECK(parsed.value.entries[0].parent_b_id == 12);
    AC_CHECK(parsed.value.entries[0].inbreeding_coefficient == 0.125);
    AC_CHECK(parsed.value.pair_coefficients.size() == 1);
    AC_CHECK(parsed.value.pair_coefficients[0].coefficient == 0.25);

    const auto main = snapshot::detail::ParseProgressUnlocks(Bytes(
        "tink_basestats tink_sexuality tink_inbreeding "
        "tink_relationships"));
    AC_CHECK(main.base_stats);
    AC_CHECK(main.sexuality);
    AC_CHECK(main.pedigree);

    const auto early = snapshot::detail::ParseProgressUnlocks(Bytes(
        "tink_basestats tink_inbreeding"));
    AC_CHECK(early.base_stats);
    AC_CHECK(!early.sexuality);
    AC_CHECK(!early.pedigree);

    const auto new_game =
        snapshot::detail::ParseProgressUnlocks(Bytes(""));
    AC_CHECK(new_game.base_stats);
    AC_CHECK(!new_game.sexuality);
    AC_CHECK(!new_game.pedigree);
}

}  // namespace autocattery::tests
