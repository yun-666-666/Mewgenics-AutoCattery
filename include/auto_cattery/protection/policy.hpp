#pragma once

#include <span>

#include "auto_cattery/protection/domain.hpp"

namespace autocattery::protection {

[[nodiscard]] ProtectionLevel Merge(
    ProtectionLevel left,
    ProtectionLevel right) noexcept;

[[nodiscard]] ProtectionDecision Evaluate(
    const ProtectionInput& input,
    std::optional<std::int64_t> current_game_day = std::nullopt);

[[nodiscard]] ProtectionDigest BuildDigest(
    std::span<const ProtectionDecision> decisions) noexcept;

[[nodiscard]] RecheckResult Recheck(
    const ProtectionDigest& preview,
    const ProtectionDigest& current) noexcept;

}  // namespace autocattery::protection
