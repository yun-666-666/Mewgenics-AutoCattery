#include "auto_cattery/snapshot/detail/cat_personality.hpp"

#include <cmath>
#include <cstring>

namespace autocattery::snapshot::detail {

void ApplyUnlockedSexuality(
    std::span<const std::uint8_t> decoded_cat,
    std::size_t equipment_start,
    CatSnapshot& cat) {
    // Format 19: breed string, 4-byte bb0, then doubles bb8 (libido)
    // and bc0 (sexuality). Locate these after the variable breed string.
    const auto offset = equipment_start + 12;
    if (offset > decoded_cat.size() ||
        sizeof(double) > decoded_cat.size() - offset) {
        return;
    }
    double libido{};
    std::memcpy(&libido, decoded_cat.data() + equipment_start + 4, sizeof(libido));
    if (std::isfinite(libido) && libido >= 0.0 && libido <= 1.0) {
        cat.libido_coefficient = libido;
        // Current native House details: e44b2/e44ce compare bb8 with .3/.7.
        cat.libido = libido < 0.3 ? CatLibido::Low
            : libido > 0.7 ? CatLibido::High : CatLibido::Normal;
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
