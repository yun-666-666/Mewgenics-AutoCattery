#pragma once

#include <string>

#include "auto_cattery/snapshot/domain.hpp"

namespace autocattery::protection {

[[nodiscard]] std::string StableCatIdentityToken(
    const snapshot::CatSnapshot& cat);

}  // namespace autocattery::protection
