#include "auto_cattery/breeding/breeding_scorer.hpp"

#include <algorithm>
#include <cmath>
#include <string_view>

namespace autocattery::breeding {
namespace {

constexpr std::array<std::string_view, snapshot::kStatCount> kStatKeys{
    "strength",
    "dexterity",
    "constitution",
    "intelligence",
    "speed",
    "charisma",
    "luck"
};

constexpr std::size_t kFirstActiveSlot = 2;
constexpr std::size_t kActiveSlotEnd = 6;
constexpr std::size_t kFirstPassiveSlot = 6;
constexpr std::size_t kPassiveSlotEnd = 8;
constexpr std::size_t kFirstDisorderSlot = 8;
constexpr std::size_t kDisorderSlotEnd = 10;

bool IsPresent(std::string_view value) {
    return !value.empty() && value != "None";
}

double Weight(
    const std::unordered_map<std::string, double>& overrides,
    const std::string& key,
    double fallback) {
    const auto found = overrides.find(key);
    return found == overrides.end() ? fallback : found->second;
}

bool AllFinite(const std::unordered_map<std::string, double>& values) {
    return std::ranges::all_of(
        values,
        [](const auto& entry) {
            return !entry.first.empty() && std::isfinite(entry.second);
        });
}

void AddConfiguredSlots(
    BreedingScoreResult& result,
    const snapshot::CatSnapshot& cat,
    std::size_t begin,
    std::size_t end,
    std::string_view prefix,
    double fallback,
    const std::unordered_map<std::string, double>& overrides,
    double sign) {
    const auto bounded_end = std::min(end, cat.raw_ability_slots.size());
    for (auto index = begin; index < bounded_end; ++index) {
        const auto& value = cat.raw_ability_slots[index];
        if (!IsPresent(value)) {
            continue;
        }
        const auto configured = Weight(overrides, value, fallback);
        const auto contribution = configured * sign;
        result.score += contribution;
        result.components.push_back({
            std::string(prefix) + ":" + value,
            1.0,
            configured,
            contribution,
            "explicit configured value for a confirmed save slot"
        });
    }
}

}  // namespace

Result<void> Validate(const BreedingScoringConfig& config) {
    if (config.version != 1) {
        return {ErrorCode::ConfigInvalid, "unsupported breeding scoring version"};
    }
    if (config.minimum_known_stats > snapshot::kStatCount) {
        return {
            ErrorCode::ConfigInvalid,
            "minimum_known_stats exceeds the seven confirmed stats"
        };
    }
    if (!std::isfinite(config.minimum_score) ||
        !std::isfinite(config.missing_stat_penalty) ||
        !std::isfinite(config.active_ability_default_weight) ||
        !std::isfinite(config.passive_default_weight) ||
        !std::isfinite(config.disorder_default_penalty) ||
        !std::ranges::all_of(
            config.stat_weights,
            [](double value) { return std::isfinite(value); }) ||
        !AllFinite(config.active_ability_overrides) ||
        !AllFinite(config.passive_overrides) ||
        !AllFinite(config.disorder_overrides)) {
        return {ErrorCode::ConfigInvalid, "scoring values must be finite"};
    }
    return {};
}

Result<BreedingScoreResult> ScoreBreedingCat(
    const snapshot::CatSnapshot& cat,
    const BreedingScoringConfig& config) {
    const auto validation = Validate(config);
    if (!validation) {
        return {{}, validation.code, validation.message};
    }

    BreedingScoreResult result;
    result.cat_id = cat.id;
    if (cat.life_stage == snapshot::LifeStage::Dead) {
        result.exclusion_reasons.push_back("dead");
    } else if (cat.life_stage == snapshot::LifeStage::Kitten) {
        result.exclusion_reasons.push_back("kitten");
    } else if (cat.life_stage == snapshot::LifeStage::Unknown) {
        result.limitations.push_back("life stage is unavailable");
        if (config.require_confirmed_eligibility) {
            result.exclusion_reasons.push_back("life-stage-unconfirmed");
        }
    }

    if (cat.available_for_breeding == snapshot::TriState::No) {
        result.exclusion_reasons.push_back("not-available-for-breeding");
    } else if (cat.available_for_breeding == snapshot::TriState::Unknown) {
        result.limitations.push_back("breeding availability is unavailable");
        if (config.require_confirmed_eligibility) {
            result.exclusion_reasons.push_back(
                "breeding-availability-unconfirmed");
        }
    }

    std::size_t known_stats{};
    for (std::size_t index = 0; index < snapshot::kStatCount; ++index) {
        const auto genetic = cat.genetic_stats.values[index];
        if (!genetic) {
            result.score -= config.missing_stat_penalty;
            result.components.push_back({
                "missing:" + std::string(kStatKeys[index]),
                0.0,
                config.missing_stat_penalty,
                -config.missing_stat_penalty,
                "unlocked base stat is unavailable"
            });
            continue;
        }
        ++known_stats;
        const auto total = static_cast<std::int64_t>(*genetic);
        result.heritable_stat_sum += total;
        const auto contribution =
            static_cast<double>(total) * config.stat_weights[index];
        result.score += contribution;
        result.components.push_back({
            std::string(kStatKeys[index]),
            static_cast<double>(total),
            config.stat_weights[index],
            contribution,
            "unlocked inherited base stat; later modifiers excluded"
        });
    }

    if (cat.raw_ability_slots.size() == kDisorderSlotEnd) {
        AddConfiguredSlots(
            result,
            cat,
            kFirstActiveSlot,
            kActiveSlotEnd,
            "active",
            config.active_ability_default_weight,
            config.active_ability_overrides,
            1.0);
        AddConfiguredSlots(
            result,
            cat,
            kFirstPassiveSlot,
            kPassiveSlotEnd,
            "passive",
            config.passive_default_weight,
            config.passive_overrides,
            1.0);
        AddConfiguredSlots(
            result,
            cat,
            kFirstDisorderSlot,
            kDisorderSlotEnd,
            "disorder",
            config.disorder_default_penalty,
            config.disorder_overrides,
            -1.0);
    } else {
        result.limitations.push_back(
            "ten typed core ability slots are unavailable for this cat");
    }

    result.limitations.push_back(
        "relationships, fertility, sex compatibility, and rare-trait identity "
        "are not confirmed by the current adapter");
    result.confidence = static_cast<double>(known_stats) /
        static_cast<double>(snapshot::kStatCount);
    if (known_stats < config.minimum_known_stats) {
        result.exclusion_reasons.push_back("insufficient-confirmed-stats");
    }
    if (result.score < config.minimum_score) {
        result.exclusion_reasons.push_back("below-minimum-breeding-score");
    }
    result.eligible = result.exclusion_reasons.empty();
    return {std::move(result)};
}

}  // namespace autocattery::breeding
