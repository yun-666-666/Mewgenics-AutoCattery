#pragma once
#include <algorithm>
#include <map>
#include <span>
#include "auto_cattery/breeding/domain.hpp"

namespace autocattery::breeding {
// Return the best ranked pair that can coexist with a second unrelated family.
// Unknown ancestry is not evidence of independence.
inline std::vector<snapshot::CatId> IndependentBreedingFamilies(
    std::span<const BreedingPairScore> pairs,
    const snapshot::HouseSnapshot& house) {
    using Key = std::pair<snapshot::CatId, snapshot::CatId>;
    const auto key = [](auto a, auto b) -> Key { return std::minmax(a, b); };
    std::map<Key, double> coi;
    for (const auto& entry : house.pedigree_pair_coefficients)
        coi[key(entry.cat_a_id, entry.cat_b_id)] = entry.coefficient;
    const auto unrelated = [&](auto a, auto b) {
        const auto found = coi.find(key(a, b));
        return a != b && found != coi.end() && found->second == 0.0;
    };
    for (std::size_t i = 0; i < pairs.size(); ++i) {
        const auto& first = pairs[i];
        if (!first.eligible || !unrelated(first.cat_a_id, first.cat_b_id)) continue;
        for (std::size_t j = i + 1; j < pairs.size(); ++j) {
            const auto& second = pairs[j];
            if (second.eligible && unrelated(second.cat_a_id, second.cat_b_id) &&
                unrelated(first.cat_a_id, second.cat_a_id) &&
                unrelated(first.cat_a_id, second.cat_b_id) &&
                unrelated(first.cat_b_id, second.cat_a_id) &&
                unrelated(first.cat_b_id, second.cat_b_id))
                return {first.cat_a_id, first.cat_b_id, second.cat_a_id, second.cat_b_id};
        }
    }
    return {};
}
}
