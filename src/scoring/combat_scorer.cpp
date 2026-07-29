#include "auto_cattery/scoring/combat_scorer.hpp"

#include <algorithm>
#include <cmath>
#include <string_view>

namespace autocattery::scoring {
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

constexpr std::size_t kMoveSlot = 0;
constexpr std::size_t kAttackSlot = 1;
constexpr std::size_t kFirstActiveSlot = 2;
constexpr std::size_t kActiveSlotEnd = 6;
constexpr std::size_t kFirstPassiveSlot = 6;
constexpr std::size_t kPassiveSlotEnd = 8;
constexpr std::size_t kFirstDisorderSlot = 8;
constexpr std::size_t kDisorderSlotEnd = 10;

bool IsPresentAbility(std::string_view ability) {
    return !ability.empty() && ability != "None";
}

double OverrideWeight(
    const std::unordered_map<std::string, double>& overrides,
    const std::string& ability,
    double fallback) {
    const auto found = overrides.find(ability);
    return found == overrides.end() ? fallback : found->second;
}

void AddAbilityComponents(
    CombatScoreResult& result,
    const snapshot::CatSnapshot& cat,
    std::size_t begin,
    std::size_t end,
    std::string_view prefix,
    double fallback_weight,
    const std::unordered_map<std::string, double>& overrides,
    double sign) {
    const auto bounded_end = std::min(end, cat.raw_ability_slots.size());
    for (auto index = begin; index < bounded_end; ++index) {
        const auto& ability = cat.raw_ability_slots[index];
        if (!IsPresentAbility(ability)) {
            continue;
        }
        const auto configured_weight =
            OverrideWeight(overrides, ability, fallback_weight);
        const auto contribution = configured_weight * sign;
        result.score += contribution;
        result.components.push_back({
            std::string(prefix) + ":" + ability,
            1.0,
            configured_weight,
            contribution,
            "confirmed save ability slot"
        });
    }
}

bool AllFinite(const std::unordered_map<std::string, double>& values) {
    return std::ranges::all_of(
        values,
        [](const auto& entry) {
            return !entry.first.empty() && std::isfinite(entry.second);
        });
}

}  // namespace

Result<void> Validate(const CombatScoringConfig& config) {
    if (config.version != 1) {
        return {ErrorCode::ConfigInvalid, "unsupported combat scoring version"};
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
        !std::isfinite(config.injury_penalty) ||
        !std::ranges::all_of(
            config.stat_weights,
            [](double value) {
                return std::isfinite(value);
            }) ||
        !AllFinite(config.active_ability_overrides) ||
        !AllFinite(config.passive_overrides) ||
        !AllFinite(config.disorder_overrides)) {
        return {ErrorCode::ConfigInvalid, "scoring values must be finite"};
    }
    return {};
}

Result<CombatScoreResult> ScoreCombatCat(
    const snapshot::CatSnapshot& cat,
    const CombatScoringConfig& config) {
    const auto validation = Validate(config);
    if (!validation) {
        return {{}, validation.code, validation.message};
    }

    CombatScoreResult result;
    result.cat_id = cat.id;
    if (cat.life_stage == snapshot::LifeStage::Dead) {
        result.exclusion_reasons.push_back("dead");
    }
    if (config.exclude_kittens &&
        cat.life_stage == snapshot::LifeStage::Kitten) {
        result.exclusion_reasons.push_back("kitten");
    }
    if (cat.available_for_combat == snapshot::TriState::No) {
        result.exclusion_reasons.push_back("not-available-for-combat");
    } else if (cat.available_for_combat == snapshot::TriState::Unknown) {
        result.limitations.push_back("combat availability is unavailable");
        if (config.require_confirmed_eligibility) {
            result.exclusion_reasons.push_back(
                "combat-availability-unconfirmed");
        }
    }
    if (cat.injured == snapshot::TriState::Yes) {
        if (config.exclude_injured) {
            result.exclusion_reasons.push_back("injured");
        } else {
            result.score -= config.injury_penalty;
            result.components.push_back({
                "injury",
                1.0,
                config.injury_penalty,
                -config.injury_penalty,
                "confirmed injury penalty"
            });
        }
    } else if (cat.injured == snapshot::TriState::Unknown) {
        result.limitations.push_back("injury state is unavailable");
        if (config.exclude_injured &&
            config.require_confirmed_eligibility) {
            result.exclusion_reasons.push_back("injury-state-unconfirmed");
        }
    }
    if (cat.life_stage == snapshot::LifeStage::Unknown) {
        result.limitations.push_back("life stage is unavailable");
        if (config.exclude_kittens &&
            config.require_confirmed_eligibility) {
            result.exclusion_reasons.push_back("life-stage-unconfirmed");
        }
    }

    std::size_t known_stats{};
    for (std::size_t index = 0; index < snapshot::kStatCount; ++index) {
        const auto genetic = cat.genetic_stats.values[index];
        const auto heredity = cat.heredity_bonus.values[index];
        const auto equipment = cat.equipment_bonus.values[index];
        if (!genetic || !heredity || !equipment) {
            result.score -= config.missing_stat_penalty;
            result.components.push_back({
                "missing:" + std::string(kStatKeys[index]),
                0.0,
                config.missing_stat_penalty,
                -config.missing_stat_penalty,
                "one or more confirmed stat sources are unavailable"
            });
            continue;
        }

        ++known_stats;
        const auto total = static_cast<std::int64_t>(*genetic) +
            static_cast<std::int64_t>(*heredity) +
            static_cast<std::int64_t>(*equipment);
        result.total_stat_sum += total;
        const auto contribution =
            static_cast<double>(total) * config.stat_weights[index];
        result.score += contribution;
        result.components.push_back({
            std::string(kStatKeys[index]),
            static_cast<double>(total),
            config.stat_weights[index],
            contribution,
            "genetic + heredity bonus + equipment bonus"
        });
    }

    if (cat.raw_ability_slots.size() == kDisorderSlotEnd) {
        if (IsPresentAbility(cat.raw_ability_slots[kMoveSlot])) {
            result.limitations.push_back(
                "movement ability is identified but has no universal default value");
        }
        if (IsPresentAbility(cat.raw_ability_slots[kAttackSlot])) {
            result.limitations.push_back(
                "basic attack is identified but has no universal default value");
        }
        AddAbilityComponents(
            result,
            cat,
            kFirstActiveSlot,
            kActiveSlotEnd,
            "active",
            config.active_ability_default_weight,
            config.active_ability_overrides,
            1.0);
        AddAbilityComponents(
            result,
            cat,
            kFirstPassiveSlot,
            kPassiveSlotEnd,
            "passive",
            config.passive_default_weight,
            config.passive_overrides,
            1.0);
        AddAbilityComponents(
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

    result.confidence = static_cast<double>(known_stats) /
        static_cast<double>(snapshot::kStatCount);
    if (known_stats < config.minimum_known_stats) {
        result.exclusion_reasons.push_back("insufficient-confirmed-stats");
    }
    result.eligible = result.exclusion_reasons.empty();
    return {std::move(result)};
}

}  // namespace autocattery::scoring
