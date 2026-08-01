#include "auto_cattery/protection/identity.hpp"

#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string_view>

namespace autocattery::protection {
namespace {

constexpr std::uint64_t kOffset = 14695981039346656037ULL;
constexpr std::uint64_t kPrime = 1099511628211ULL;

void AppendByte(std::uint64_t& hash, std::uint8_t value) {
    hash ^= value;
    hash *= kPrime;
}

void AppendU64(std::uint64_t& hash, std::uint64_t value) {
    for (unsigned shift = 0; shift < 64; shift += 8) {
        AppendByte(hash, static_cast<std::uint8_t>(value >> shift));
    }
}

void AppendText(std::uint64_t& hash, std::string_view value) {
    AppendU64(hash, value.size());
    for (const unsigned char byte : value) {
        AppendByte(hash, byte);
    }
}

void AppendOptional(
    std::uint64_t& hash,
    const std::optional<std::int64_t>& value) {
    AppendByte(hash, value.has_value());
    if (value) {
        AppendU64(hash, static_cast<std::uint64_t>(*value));
    }
}

}  // namespace

std::string StableCatIdentityToken(const snapshot::CatSnapshot& cat) {
    std::uint64_t hash = kOffset;
    AppendText(hash, "AutoCatteryCatIdentityV1");
    AppendU64(hash, static_cast<std::uint64_t>(cat.id));
    AppendOptional(hash, cat.birth_day);
    AppendText(hash, cat.breed_id);
    AppendText(hash, cat.voice_id);
    std::ostringstream result;
    result << "cat-v1-" << std::hex << std::setfill('0')
           << std::setw(16) << hash;
    return result.str();
}

}  // namespace autocattery::protection
