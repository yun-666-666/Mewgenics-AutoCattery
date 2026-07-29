#pragma once

#include <string>
#include <string_view>

#include "auto_cattery/classification/domain.hpp"
#include "auto_cattery/config.hpp"

namespace autocattery::workflow {

[[nodiscard]] std::string DigestPrivateIdentity(std::string_view value);
[[nodiscard]] std::string DigestConfig(const Config &config);
[[nodiscard]] std::string
DigestCandidateOrder(const classification::ClassificationPlan &plan);

} // namespace autocattery::workflow
