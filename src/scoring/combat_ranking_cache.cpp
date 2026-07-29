#include "auto_cattery/scoring/combat_ranking_cache.hpp"

#include <algorithm>
#include <bit>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "auto_cattery/scoring/combat_ranker.hpp"
#include "auto_cattery/scoring/combat_scorer.hpp"

namespace autocattery::scoring {
namespace {

class StableHash final {
public:
    void AddBytes(std::span<const std::byte> bytes) noexcept {
        for (const auto value : bytes) {
            value_ ^= static_cast<std::uint8_t>(value);
            value_ *= 1'099'511'628'211ULL;
        }
    }

    template<class T>
    void Add(const T& value) noexcept {
        AddBytes(std::as_bytes(std::span{&value, 1}));
    }

    void Add(std::string_view value) noexcept {
        AddBytes(std::as_bytes(std::span{value.data(), value.size()}));
        constexpr std::uint8_t separator = 0xFF;
        Add(separator);
    }

    [[nodiscard]] std::uint64_t Value() const noexcept {
        return value_;
    }

private:
    std::uint64_t value_{14'695'981'039'346'656'037ULL};
};

void AddOverrides(
    StableHash& hash,
    const std::unordered_map<std::string, double>& values) {
    std::vector<std::pair<std::string_view, double>> ordered;
    ordered.reserve(values.size());
    for (const auto& [key, value] : values) {
        ordered.emplace_back(key, value);
    }
    std::sort(
        ordered.begin(),
        ordered.end(),
        [](const auto& left, const auto& right) {
            return left.first < right.first;
        });
    for (const auto& [key, value] : ordered) {
        hash.Add(key);
        hash.Add(std::bit_cast<std::uint64_t>(value));
    }
}

}  // namespace

Result<std::uint64_t> CombatConfigHash(
    const CombatScoringConfig& config) {
    const auto validation = Validate(config);
    if (!validation) {
        return {0, validation.code, validation.message};
    }

    StableHash hash;
    hash.Add(config.version);
    hash.Add(config.recommended_count);
    hash.Add(std::bit_cast<std::uint64_t>(config.minimum_score));
    hash.Add(config.minimum_known_stats);
    hash.Add(config.exclude_kittens);
    hash.Add(config.exclude_injured);
    hash.Add(config.require_confirmed_eligibility);
    for (const auto weight : config.stat_weights) {
        hash.Add(std::bit_cast<std::uint64_t>(weight));
    }
    hash.Add(std::bit_cast<std::uint64_t>(config.missing_stat_penalty));
    hash.Add(std::bit_cast<std::uint64_t>(
        config.active_ability_default_weight));
    hash.Add(std::bit_cast<std::uint64_t>(config.passive_default_weight));
    hash.Add(std::bit_cast<std::uint64_t>(
        config.disorder_default_penalty));
    hash.Add(std::bit_cast<std::uint64_t>(config.injury_penalty));
    hash.Add("active");
    AddOverrides(hash, config.active_ability_overrides);
    hash.Add("passive");
    AddOverrides(hash, config.passive_overrides);
    hash.Add("disorder");
    AddOverrides(hash, config.disorder_overrides);
    return {hash.Value()};
}

Result<CombatRanking> CombatRankingCache::GetOrCompute(
    const snapshot::HouseSnapshot& snapshot,
    const CombatScoringConfig& config) {
    const auto config_hash = CombatConfigHash(config);
    if (!config_hash) {
        return {{}, config_hash.code, config_hash.message};
    }
    const Key requested{
        .snapshot_id = snapshot.snapshot_id,
        .config_hash = config_hash.value
    };
    if (key_ == requested && ranking_) {
        return {*ranking_};
    }

    auto computed = RankCombatCats(snapshot, config);
    if (!computed) {
        return computed;
    }
    key_ = requested;
    ranking_ = computed.value;
    ++computation_count_;
    return computed;
}

void CombatRankingCache::Clear() noexcept {
    key_.reset();
    ranking_.reset();
}

std::size_t CombatRankingCache::ComputationCount() const noexcept {
    return computation_count_;
}

}  // namespace autocattery::scoring
