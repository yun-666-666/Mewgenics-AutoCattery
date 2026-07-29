#include "auto_cattery/snapshot/detail/lz4_block.hpp"

#include <limits>

namespace autocattery::snapshot::detail {
namespace {

constexpr std::uint32_t kCatMagic = 19;
constexpr std::size_t kMaximumRawSize = 64 * 1024;

std::uint32_t ReadU32(std::span<const std::uint8_t> bytes) {
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8U) |
           (static_cast<std::uint32_t>(bytes[2]) << 16U) |
           (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

bool ReadExtendedLength(
    std::span<const std::uint8_t> source,
    std::size_t& cursor,
    std::size_t& length) {
    while (cursor < source.size()) {
        const auto extension = source[cursor++];
        if (length > std::numeric_limits<std::size_t>::max() - extension) {
            return false;
        }
        length += extension;
        if (extension != 255) {
            return true;
        }
    }
    return false;
}

}  // namespace

Result<std::vector<std::uint8_t>> DecodeCatStorageBlob(
    std::span<const std::uint8_t> stored) {
    if (stored.size() >= 4 && ReadU32(stored.first<4>()) == kCatMagic) {
        return {std::vector<std::uint8_t>(stored.begin(), stored.end())};
    }
    if (stored.size() < 5) {
        return {{}, ErrorCode::CatDataUnavailable, "cat blob is too short"};
    }

    const auto expected_size = ReadU32(stored.first<4>());
    if (expected_size < 20 || expected_size > kMaximumRawSize) {
        return {
            {},
            ErrorCode::CatDataUnavailable,
            "cat blob has an invalid decompressed size"
        };
    }

    const auto source = stored.subspan(4);
    std::vector<std::uint8_t> output;
    output.reserve(expected_size);
    std::size_t cursor{};
    while (cursor < source.size()) {
        const auto token = source[cursor++];
        std::size_t literal_length = token >> 4U;
        if (literal_length == 15 &&
            !ReadExtendedLength(source, cursor, literal_length)) {
            return {{}, ErrorCode::CatDataUnavailable, "invalid LZ4 literal length"};
        }
        if (literal_length > source.size() - cursor ||
            literal_length > expected_size - output.size()) {
            return {{}, ErrorCode::CatDataUnavailable, "LZ4 literal exceeds bounds"};
        }
        output.insert(
            output.end(),
            source.begin() + static_cast<std::ptrdiff_t>(cursor),
            source.begin() + static_cast<std::ptrdiff_t>(
                cursor + literal_length));
        cursor += literal_length;
        if (cursor == source.size()) {
            break;
        }
        if (source.size() - cursor < 2) {
            return {{}, ErrorCode::CatDataUnavailable, "LZ4 match offset is truncated"};
        }
        const auto offset =
            static_cast<std::size_t>(source[cursor]) |
            (static_cast<std::size_t>(source[cursor + 1]) << 8U);
        cursor += 2;
        if (offset == 0 || offset > output.size()) {
            return {{}, ErrorCode::CatDataUnavailable, "LZ4 match offset is invalid"};
        }
        std::size_t match_length = token & 0x0FU;
        if (match_length == 15 &&
            !ReadExtendedLength(source, cursor, match_length)) {
            return {{}, ErrorCode::CatDataUnavailable, "invalid LZ4 match length"};
        }
        match_length += 4;
        if (match_length > expected_size - output.size()) {
            return {{}, ErrorCode::CatDataUnavailable, "LZ4 match exceeds bounds"};
        }
        for (std::size_t index = 0; index < match_length; ++index) {
            output.push_back(output[output.size() - offset]);
        }
    }
    if (output.size() != expected_size ||
        output.size() < 4 ||
        ReadU32(std::span<const std::uint8_t>(output).first<4>()) !=
            kCatMagic) {
        return {{}, ErrorCode::CatDataUnavailable, "decoded cat blob failed validation"};
    }
    return {std::move(output)};
}

}  // namespace autocattery::snapshot::detail
