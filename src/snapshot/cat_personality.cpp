#include "auto_cattery/snapshot/detail/cat_personality.hpp"

#include <cmath>
#include <cstring>

namespace autocattery::snapshot::detail {

void ApplyUnlockedSexuality(
    std::span<const std::uint8_t> decoded_cat,
    std::size_t personality_anchor,
    CatSnapshot& cat) {
    constexpr std::size_t kSexualityOffset = 40;
    const auto offset = personality_anchor + kSexualityOffset;
    if (offset > decoded_cat.size() ||
        sizeof(double) > decoded_cat.size() - offset) {
        return;
    }
    double value{};
    std::memcpy(&value, decoded_cat.data() + offset, sizeof(value));
    if (!std::isfinite(value) || value < 0.0 || value > 1.0) {
        return;
    }
    cat.sexuality_coefficient = value;
    if (value < 0.1) {
        cat.sexuality = CatSexuality::Straight;
    } else if (value >= 0.9) {
        cat.sexuality = CatSexuality::Gay;
    } else {
        cat.sexuality = CatSexuality::Bisexual;
    }
}

}  // namespace autocattery::snapshot::detail
