#include "pair_trait_scorer.hpp"

#include <algorithm>
#include <array>
#include <string_view>
#include <tuple>

namespace autocattery::breeding::detail {
namespace {

constexpr std::size_t kActiveBegin = 2;
constexpr std::size_t kActiveEnd = 6;
constexpr std::size_t kPassiveBegin = 6;
constexpr std::size_t kPassiveEnd = 8;
constexpr std::size_t kDisorderBegin = 8;
constexpr std::size_t kDisorderEnd = 10;
constexpr std::array<std::string_view, 10> kVisualGroups{
    "fur", "body", "head", "tail", "legs",
    "arms", "eyes", "eyebrows", "ears", "mouth"
};

bool Present(std::string_view value) {
    return !value.empty() && value != "None";
}

double Weight(
    const std::unordered_map<std::string, double>& overrides,
    const std::string& key,
    double fallback) {
    const auto found = overrides.find(key);
    return found == overrides.end() ? fallback : found->second;
}

double MeanSlots(
    const snapshot::CatSnapshot& cat,
    std::size_t begin,
    std::size_t end,
    const std::unordered_map<std::string, double>& overrides,
    double fallback,
    bool exclude_skill_share = false) {
    double total{};
    std::size_t count{};
    const auto bounded_end = std::min(end, cat.raw_ability_slots.size());
    for (auto index = begin; index < bounded_end; ++index) {
        const auto& value = cat.raw_ability_slots[index];
        if (!Present(value) || (exclude_skill_share && value == "SkillShare")) {
            continue;
        }
        total += Weight(overrides, value, fallback);
        ++count;
    }
    return count == 0 ? 0.0 : total / static_cast<double>(count);
}

std::string VisualGroup(std::string_view slot) {
    if (slot.starts_with("leg_")) return "legs";
    if (slot.starts_with("arm_")) return "arms";
    if (slot.starts_with("eye_")) return "eyes";
    if (slot.starts_with("eyebrow_")) return "eyebrows";
    if (slot.starts_with("ear_")) return "ears";
    return std::string(slot);
}

double VisualGroupValue(
    const snapshot::CatSnapshot& cat,
    std::string_view group,
    const BreedingScoringConfig& config,
    bool& present) {
    double total{};
    std::size_t count{};
    for (const auto& trait : cat.visual_traits) {
        if (VisualGroup(trait.slot) != group) {
            continue;
        }
        const auto key = trait.category + ":" + std::to_string(trait.id);
        if (trait.kind == snapshot::VisualTraitKind::BirthDefect) {
            total -= Weight(
                config.birth_defect_overrides,
                key,
                config.birth_defect_default_penalty);
        } else {
            total += Weight(
                config.mutation_overrides,
                key,
                config.mutation_default_weight);
        }
        ++count;
    }
    present = count != 0;
    return present ? total / static_cast<double>(count) : 0.0;
}

double BestStimulation(const snapshot::HouseSnapshot& house) {
    const snapshot::RoomAttributes* preferred{};
    for (const auto& room : house.rooms) {
        if (room.attributes && room.attributes->breed_suppression <= .99) {
            const auto& attributes = *room.attributes;
            if (!preferred || std::tuple{attributes.comfort, attributes.stimulation, attributes.health} >
                std::tuple{preferred->comfort, preferred->stimulation, preferred->health}) {
                preferred = &attributes;
            }
        }
    }
    return preferred ? preferred->stimulation : 0.0;
}

double InheritanceChance(double base, double scale, double stimulation) {
    return std::clamp(base + scale * stimulation, 0.0, 1.0);
}

double BalancedValue(double value) {
    return value > 0.0 ? value / (1.0 + value) : value;
}

}  // namespace

double StablePairTraitScore(
    const snapshot::HouseSnapshot& house,
    const snapshot::CatSnapshot& a,
    const snapshot::CatSnapshot& b,
    const BreedingScoringConfig& config) {
    const auto stimulation = std::max(0.0, BestStimulation(house));
    double score{};
    if (house.capabilities.read_raw_ability_slots) {
        const auto active_mean = 0.5 * (
            MeanSlots(a, kActiveBegin, kActiveEnd,
                      config.active_ability_overrides,
                      config.active_ability_default_weight) +
            MeanSlots(b, kActiveBegin, kActiveEnd,
                      config.active_ability_overrides,
                      config.active_ability_default_weight));
        score += BalancedValue(active_mean * (
            InheritanceChance(0.20, 0.025, stimulation) +
            InheritanceChance(0.02, 0.005, stimulation)));
        const auto passive_mean = 0.5 * (
            MeanSlots(a, kPassiveBegin, kPassiveEnd,
                      config.passive_overrides,
                      config.passive_default_weight, true) +
            MeanSlots(b, kPassiveBegin, kPassiveEnd,
                      config.passive_overrides,
                      config.passive_default_weight, true));
        score += BalancedValue(passive_mean *
            InheritanceChance(0.05, 0.01, stimulation));
        score -= 0.15 * (
            MeanSlots(a, kDisorderBegin, kDisorderEnd,
                      config.disorder_overrides,
                      config.disorder_default_penalty) +
            MeanSlots(b, kDisorderBegin, kDisorderEnd,
                      config.disorder_overrides,
                      config.disorder_default_penalty));
    }
    if (!house.capabilities.read_visual_traits) {
        return score;
    }
    const auto preferred = (1.0 + 0.01 * stimulation) /
        (2.0 + 0.01 * stimulation);
    double mutations{};
    for (const auto group : kVisualGroups) {
        bool a_present{};
        bool b_present{};
        const auto av = VisualGroupValue(a, group, config, a_present);
        const auto bv = VisualGroupValue(b, group, config, b_present);
        const auto value = a_present && b_present ? 0.5 * (av + bv)
            : a_present ? preferred * av : b_present ? preferred * bv : 0.0;
        if (value > 0.0) mutations += value;
        else score += value;
    }
    return score + BalancedValue(mutations);
}

}  // namespace autocattery::breeding::detail
