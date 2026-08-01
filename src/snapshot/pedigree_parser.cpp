#include "auto_cattery/snapshot/detail/pedigree_parser.hpp"

#include <cmath>
#include <cstring>
#include <limits>
#include <optional>

namespace autocattery::snapshot::detail {
namespace {

constexpr std::uint64_t kVersionSentinel = 0xfffffffffffffff5ULL;
constexpr std::uint64_t kMaximumCapacity = 1'000'000;

template<class T>
bool Read(std::span<const std::byte> blob, std::size_t offset, T& value) {
    if (offset > blob.size() || sizeof(T) > blob.size() - offset) {
        return false;
    }
    std::memcpy(&value, blob.data() + offset, sizeof(T));
    return true;
}

struct Table {
    std::size_t controls{};
    std::size_t data{};
    std::size_t next{};
    std::uint64_t size{};
    std::uint64_t capacity{};
};

std::optional<Table> ReadTable(
    std::span<const std::byte> blob,
    std::size_t offset,
    std::size_t row_size) {
    std::uint64_t first{};
    if (!Read(blob, offset, first)) {
        return std::nullopt;
    }
    const bool versioned = first >= kVersionSentinel;
    const auto header = versioned ? 24U : 16U;
    std::uint64_t size{};
    std::uint64_t capacity{};
    if (!Read(blob, offset + (versioned ? 8U : 0U), size) ||
        !Read(blob, offset + (versioned ? 16U : 8U), capacity) ||
        capacity > kMaximumCapacity || size > capacity) {
        return std::nullopt;
    }
    const auto controls = offset + header;
    const auto control_size = capacity + 17U;
    if (controls > blob.size() || control_size > blob.size() - controls) {
        return std::nullopt;
    }
    const auto data = controls + static_cast<std::size_t>(control_size);
    if (capacity > (std::numeric_limits<std::size_t>::max() - 8U) /
            row_size ||
        static_cast<std::size_t>(capacity) * row_size + 8U >
            blob.size() - data) {
        return std::nullopt;
    }
    return Table{
        controls,
        data,
        data + static_cast<std::size_t>(capacity) * row_size + 8U,
        size,
        capacity
    };
}

bool Live(std::span<const std::byte> blob, const Table& table, std::size_t i) {
    return std::to_integer<unsigned char>(blob[table.controls + i]) <= 0x7f;
}

std::optional<CatId> Parent(std::int64_t value) {
    return value > 0 ? std::optional<CatId>(value) : std::nullopt;
}

}  // namespace

Result<PedigreeData> ParsePedigreeBlob(std::span<const std::byte> blob) {
    const auto pedigree = ReadTable(blob, 0, 32);
    if (!pedigree) {
        return {{}, ErrorCode::CatDataUnavailable, "pedigree table is invalid"};
    }
    PedigreeData result;
    for (std::size_t i = 0; i < pedigree->capacity; ++i) {
        if (!Live(blob, *pedigree, i)) {
            continue;
        }
        const auto row = pedigree->data + i * 32U;
        std::int64_t cat{};
        std::int64_t parent_a{};
        std::int64_t parent_b{};
        double coefficient{};
        if (!Read(blob, row, cat) || !Read(blob, row + 8, parent_a) ||
            !Read(blob, row + 16, parent_b) ||
            !Read(blob, row + 24, coefficient) || cat <= 0 ||
            !std::isfinite(coefficient)) {
            return {{}, ErrorCode::CatDataUnavailable, "pedigree row is invalid"};
        }
        result.entries.push_back({
            cat, Parent(parent_a), Parent(parent_b), coefficient
        });
    }
    const auto pairs = ReadTable(blob, pedigree->next, 24);
    if (!pairs) {
        return {{}, ErrorCode::CatDataUnavailable, "pedigree pair table is invalid"};
    }
    for (std::size_t i = 0; i < pairs->capacity; ++i) {
        if (!Live(blob, *pairs, i)) {
            continue;
        }
        const auto row = pairs->data + i * 24U;
        std::int64_t a{};
        std::int64_t b{};
        double coefficient{};
        if (!Read(blob, row, a) || !Read(blob, row + 8, b) ||
            !Read(blob, row + 16, coefficient) ||
            !std::isfinite(coefficient) || coefficient < 0.0) {
            return {{}, ErrorCode::CatDataUnavailable, "pedigree pair row is invalid"};
        }
        if (a <= 0 || b <= 0) {
            continue;
        }
        result.pair_coefficients.push_back({a, b, coefficient});
    }
    const auto accessible = ReadTable(blob, pairs->next, 8);
    if (!accessible || accessible->next != blob.size()) {
        return {{}, ErrorCode::CatDataUnavailable, "pedigree tail is invalid"};
    }
    return {std::move(result)};
}

}  // namespace autocattery::snapshot::detail
