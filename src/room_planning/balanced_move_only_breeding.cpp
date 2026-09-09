#include "balanced_move_only_internal.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <tuple>
#include <unordered_set>

namespace autocattery::room_planning::balanced_internal {
namespace {

bool SexMatches(const snapshot::CatSnapshot& cat, SlotSex required) {
    return required == SlotSex::Any ||
        (required == SlotSex::Female &&
         cat.sex == snapshot::CatSex::Female) ||
        (required == SlotSex::Male && cat.sex == snapshot::CatSex::Male);
}

void AddUnique(std::vector<std::string>& values, std::string value) {
    if (std::ranges::find(values, value) == values.end()) {
        values.push_back(std::move(value));
    }
}

}  // namespace

std::optional<snapshot::RoomId> FindBreedingTarget(
    const PlanningContext& context,
    const CountMap& occupancy,
    const std::optional<snapshot::RoomId>& excluded_room) {
    if (context.breeding_pair.size() != 2) {
        return std::nullopt;
    }
    std::optional<snapshot::RoomId> fixed_target;
    for (const auto cat_id : context.breeding_pair) {
        const auto fixed = context.fixed_rooms.find(cat_id);
        if (fixed == context.fixed_rooms.end()) {
            continue;
        }
        if (fixed_target && *fixed_target != fixed->second) {
            return std::nullopt;
        }
        fixed_target = fixed->second;
    }
    std::optional<snapshot::RoomId> target;
    for (const auto& room_id : context.rooms) {
        if (fixed_target && room_id != *fixed_target) {
            continue;
        }
        if (!fixed_target && excluded_room && room_id == *excluded_room) {
            continue;
        }
        if (occupancy.at(room_id) < 2 ||
            occupancy.at(room_id) - context.pinned_count.at(room_id) < 2) {
            continue;
        }
        if (!target || PreferBreedingRoom(context, room_id, *target)) {
            target = room_id;
        }
    }
    return target;
}

void AssignBreedingPairSlots(
    const PlanningContext& context,
    const std::optional<snapshot::RoomId>& target,
    RoomPlan& plan,
    std::vector<BalancedSlot>& slots) {
    if (context.breeding_pair.size() != 2) {
        return;
    }
    if (!target) {
        AddUnique(plan.limitations, "breeding-pair-room-unavailable");
        return;
    }
    for (const auto cat_id : context.breeding_pair) {
        const auto& cat = *context.cats.at(cat_id);
        const auto existing = std::ranges::find_if(
            slots,
            [&](const auto& candidate) {
                return candidate.preferred_cat &&
                    *candidate.preferred_cat == cat_id;
            });
        if (existing != slots.end()) {
            if (existing->room_id != *target) {
                AddUnique(
                    plan.limitations,
                    "breeding-pair-fixed-rooms-conflict");
            }
            continue;
        }
        auto slot = std::ranges::find_if(
            slots,
            [&](const auto& candidate) {
                return candidate.room_id == *target &&
                    !candidate.preferred_cat &&
                    !candidate.kitten_preferred &&
                    SexMatches(cat, candidate.required_sex);
            });
        if (slot == slots.end()) {
            slot = std::ranges::find_if(
                slots,
                [&](const auto& candidate) {
                    return candidate.room_id == *target &&
                        !candidate.preferred_cat &&
                        SexMatches(cat, candidate.required_sex);
                });
        }
        if (slot == slots.end()) {
            AddUnique(plan.limitations, "breeding-pair-sex-slot-unavailable");
            return;
        }
        slot->preferred_cat = cat_id;
    }
}

void AssignBreedingPoolSlots(
    const PlanningContext& context,
    const std::optional<snapshot::RoomId>& target,
    RoomPlan& plan,
    std::vector<BalancedSlot>& slots) {
    if (!target || !context.breeding_pair_preferences ||
        context.breeding_pair.size() != 2) {
        return;
    }

    const auto coverage = [&](snapshot::CatId a, snapshot::CatId b) {
        std::size_t count{};
        const auto& left = context.cats.at(a)->genetic_stats.values;
        const auto& right = context.cats.at(b)->genetic_stats.values;
        for (std::size_t i = 0; i < snapshot::kStatCount; ++i) {
            count += left[i] == 7 || right[i] == 7 ? 1U : 0U;
        }
        return count;
    };
    const auto target_coverage = coverage(
        context.breeding_pair[0], context.breeding_pair[1]);

    const auto movable = [&](snapshot::CatId cat_id) {
        return std::ranges::find(context.movable, cat_id) !=
            context.movable.end();
    };
    const auto preferred_in_target = [&](snapshot::CatId cat_id) {
        return std::ranges::any_of(
            slots,
            [&](const auto& slot) {
                return slot.room_id == *target &&
                    slot.preferred_cat &&
                    *slot.preferred_cat == cat_id;
            });
    };
    const auto available = [&](snapshot::CatId cat_id) {
        const auto fixed = context.fixed_rooms.find(cat_id);
        if (fixed != context.fixed_rooms.end()) {
            return fixed->second == *target;
        }
        if (movable(cat_id)) {
            return true;
        }
        const auto cat = context.cats.find(cat_id);
        return cat != context.cats.end() &&
            cat->second->room_id &&
            *cat->second->room_id == *target;
    };
    const auto find_slot = [&context, &slots, &target](
            snapshot::CatId cat_id) {
        const auto& cat = *context.cats.at(cat_id);
        auto best = slots.size();
        auto best_rank = std::numeric_limits<int>::max();
        for (std::size_t index = 0; index < slots.size(); ++index) {
            const auto& slot = slots[index];
            if (slot.room_id != *target ||
                slot.preferred_cat ||
                slot.kitten_preferred ||
                !SexMatches(cat, slot.required_sex)) {
                continue;
            }
            const auto rank =
                (slot.required_sex == SlotSex::Any ? 1 : 0) +
                (slot.potential_preferred ? 2 : 0);
            if (rank < best_rank) {
                best = index;
                best_rank = rank;
            }
        }
        return best;
    };

    using PairKey = std::pair<snapshot::CatId, snapshot::CatId>;
    const auto pair_key = [](snapshot::CatId left, snapshot::CatId right) {
        return left < right ? PairKey{left, right} : PairKey{right, left};
    };
    std::map<PairKey, const classification::BreedingPairPreference*>
        preferences;
    std::vector<snapshot::CatId> candidates;
    for (const auto& pair : *context.breeding_pair_preferences) {
        preferences.emplace(pair_key(pair.cat_a_id, pair.cat_b_id), &pair);
        candidates.push_back(pair.cat_a_id);
        candidates.push_back(pair.cat_b_id);
    }
    std::sort(candidates.begin(), candidates.end());
    candidates.erase(
        std::unique(candidates.begin(), candidates.end()),
        candidates.end());

    std::unordered_set<snapshot::CatId> selected(
        context.breeding_pair.begin(), context.breeding_pair.end());
    for (const auto& slot : slots) {
        if (slot.room_id == *target && slot.preferred_cat) {
            selected.insert(*slot.preferred_cat);
        }
    }
    for (const auto& [cat_id, cat] : context.cats) {
        if (!movable(cat_id) && cat->room_id &&
            *cat->room_id == *target &&
            std::ranges::any_of(
                *context.breeding_pair_preferences,
                [cat_id](const auto& pair) {
                    return pair.cat_a_id == cat_id ||
                        pair.cat_b_id == cat_id;
                })) {
            selected.insert(cat_id);
        }
    }

    struct CandidateQuality {
        snapshot::CatId cat_id{};
        std::size_t slot_index{};
        double weakest_cross_score{
            std::numeric_limits<double>::infinity()};
        double average_cross_score{};
        double worst_cross_coi{};
    };
    const auto better = [](const CandidateQuality& left,
                            const CandidateQuality& right) {
        return std::tuple{
            -left.weakest_cross_score,
            left.worst_cross_coi,
            -left.average_cross_score,
            left.cat_id,
            left.slot_index
        } < std::tuple{
            -right.weakest_cross_score,
            right.worst_cross_coi,
            -right.average_cross_score,
            right.cat_id,
            right.slot_index
        };
    };

    while (true) {
        std::optional<CandidateQuality> best;
        for (const auto cat_id : candidates) {
            if (selected.contains(cat_id) || !available(cat_id) ||
                preferred_in_target(cat_id)) {
                continue;
            }
            const auto slot_index = find_slot(cat_id);
            if (slot_index == slots.size()) {
                continue;
            }
            const auto& cat = *context.cats.at(cat_id);
            CandidateQuality quality{
                .cat_id = cat_id,
                .slot_index = slot_index
            };
            double total_score{};
            std::size_t cross_pair_count{};
            bool compatible = true;
            for (const auto selected_id : selected) {
                const auto& other = *context.cats.at(selected_id);
                if (cat.sex == other.sex) {
                    continue;
                }
                const auto pair = preferences.find(
                    pair_key(cat_id, selected_id));
                if (pair == preferences.end()) {
                    compatible = false;
                    break;
                }
                const auto& preference = *pair->second;
                // Co-location permits cross-pair mating, not just the
                // displayed recommendation. Preserve its attainable stats.
                if (coverage(cat_id, selected_id) < target_coverage ||
                    (context.breeding_stats_stable &&
                     !preference.stable_all_seven)) {
                    compatible = false;
                    break;
                }
                quality.weakest_cross_score = std::min(
                    quality.weakest_cross_score,
                    preference.score);
                quality.worst_cross_coi = std::max(
                    quality.worst_cross_coi,
                    preference.offspring_inbreeding_coefficient
                        .value_or(std::numeric_limits<double>::infinity()));
                total_score += preference.score;
                ++cross_pair_count;
            }
            if (!compatible || cross_pair_count == 0) {
                continue;
            }
            quality.average_cross_score =
                total_score / static_cast<double>(cross_pair_count);
            if (!best || better(quality, *best)) {
                best = quality;
            }
        }
        if (!best) {
            break;
        }
        auto& slot = slots[best->slot_index];
        slot.preferred_cat = best->cat_id;
        slot.breeding_pool_preferred = true;
        selected.insert(best->cat_id);
    }

    // Unclaimed slots must not refill this room with unrelated breeders.
    // Pool size follows compatible cats and available space, not a fixed
    // two-cat template. Fixed/protected residents retain their assignments.
    CountMap occupancy = context.pinned_count;
    for (const auto& slot : slots) {
        ++occupancy[slot.room_id];
    }
    for (auto& slot : slots) {
        if (slot.room_id != *target || slot.preferred_cat) {
            continue;
        }
        std::optional<snapshot::RoomId> destination;
        for (const auto& room_id : context.rooms) {
            if (room_id == *target) {
                continue;
            }
            const auto capacity =
                context.capabilities.at(room_id)->confirmed_hard_capacity;
            if (capacity && occupancy.at(room_id) >= *capacity) {
                continue;
            }
            if (!destination || PreferOccupancyRoom(
                    context, room_id, *destination, occupancy)) {
                destination = room_id;
            }
        }
        if (!destination) {
            AddUnique(plan.limitations, "breeding-pool-isolation-space-unavailable");
            continue;
        }
        --occupancy.at(*target);
        ++occupancy.at(*destination);
        slot.room_id = *destination;
        slot.required_sex = SlotSex::Any;
    }
}

}  // namespace autocattery::room_planning::balanced_internal
