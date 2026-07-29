#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "auto_cattery/error.hpp"
#include "auto_cattery/scoring/domain.hpp"

namespace autocattery::scoring {

Result<std::uint64_t> CombatConfigHash(
    const CombatScoringConfig& config);

class CombatRankingCache final {
public:
    Result<CombatRanking> GetOrCompute(
        const snapshot::HouseSnapshot& snapshot,
        const CombatScoringConfig& config);

    void Clear() noexcept;
    [[nodiscard]] std::size_t ComputationCount() const noexcept;

private:
    struct Key {
        std::uint64_t snapshot_id{};
        std::uint64_t config_hash{};

        bool operator==(const Key&) const = default;
    };

    std::optional<Key> key_;
    std::optional<CombatRanking> ranking_;
    std::size_t computation_count_{};
};

}  // namespace autocattery::scoring
