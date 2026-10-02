#pragma once

#include <algorithm>
#include <map>
#include <unordered_map>
#include <unordered_set>

#include "auto_cattery/snapshot/domain.hpp"

namespace autocattery::breeding {

// Two admission places inside the cap, after protected cats and breeding
// families. Recorded founders with no living descendants represent bloodlines
// that have not yet entered this house's next generation. Missing pedigree
// entries or pair coefficients never establish an unrelated founder.
inline void ReserveUnrepresentedFounders(
    const snapshot::HouseSnapshot& house,
    const std::unordered_set<snapshot::CatId>& breeders,
    std::size_t limit,
    std::unordered_set<snapshot::CatId>& keep) {
    using snapshot::CatId;
    std::unordered_map<CatId, const snapshot::PedigreeEntry*> pedigree;
    for (const auto& entry : house.pedigree) pedigree.emplace(entry.cat_id, &entry);
    std::map<std::pair<CatId, CatId>, double> coefficients;
    for (const auto& entry : house.pedigree_pair_coefficients)
        coefficients[std::minmax(entry.cat_a_id, entry.cat_b_id)] = entry.coefficient;
    const auto unrelated = [&](CatId a, CatId b) {
        const auto found = coefficients.find(std::minmax(a, b));
        return a != b && found != coefficients.end() && found->second == 0.0;
    };
    std::unordered_set<CatId> ancestors;
    const auto visit_parents = [&](CatId id, auto&& visit) -> void {
        const auto found = pedigree.find(id);
        if (found == pedigree.end()) return;
        for (const auto parent : {found->second->parent_a_id, found->second->parent_b_id})
            if (parent && ancestors.insert(*parent).second) visit(*parent, visit);
    };
    for (const auto& cat : house.cats)
        if (cat.life_stage != snapshot::LifeStage::Dead) visit_parents(cat.id, visit_parents);

    std::vector<const snapshot::CatSnapshot*> candidates;
    for (const auto& cat : house.cats) {
        const auto record = pedigree.find(cat.id);
        if (keep.contains(cat.id) || record == pedigree.end() ||
            record->second->parent_a_id || record->second->parent_b_id ||
            record->second->inbreeding_coefficient != 0.0 || ancestors.contains(cat.id) ||
            (cat.life_stage != snapshot::LifeStage::Adult && cat.life_stage != snapshot::LifeStage::Kitten) ||
            (cat.life_stage == snapshot::LifeStage::Adult &&
                cat.available_for_breeding != snapshot::TriState::Yes) ||
            cat.injured == snapshot::TriState::Yes || !cat.sexuality_coefficient ||
            cat.libido == snapshot::CatLibido::Low || cat.libido == snapshot::CatLibido::Unknown ||
            cat.sex == snapshot::CatSex::Unknown) continue;
        bool compatible = false;
        bool independent = !breeders.empty();
        for (const auto id : breeders) {
            independent = independent && unrelated(cat.id, id);
            const auto partner = std::ranges::find(house.cats, id, &snapshot::CatSnapshot::id);
            if (partner != house.cats.end() && cat.sex != partner->sex &&
                !(cat.sexuality == snapshot::CatSexuality::Gay &&
                  partner->sexuality == snapshot::CatSexuality::Gay)) compatible = true;
        }
        if (independent && compatible) candidates.push_back(&cat);
    }
    // Age is an age preference, not an inferred arrival date. Attributes do
    // not rank these places, so weak founders can be admitted. A fixed snapshot
    // always chooses the same cats; advancing the day alone does not rotate them.
    std::ranges::sort(candidates, [](const auto* a, const auto* b) {
        const auto age_a = a->age_days.value_or(INT64_MAX);
        const auto age_b = b->age_days.value_or(INT64_MAX);
        return age_a != age_b ? age_a < age_b : a->id > b->id;
    });
    std::vector<const snapshot::CatSnapshot*> reserved;
    for (const auto* cat : candidates) {
        if (reserved.size() == 2U || keep.size() >= limit) break;
        if (std::ranges::any_of(reserved, [&](const auto* other) {
                return cat->sex == other->sex || !unrelated(cat->id, other->id);
            })) continue;
        keep.insert(cat->id);
        reserved.push_back(cat);
    }
}

}  // namespace autocattery::breeding
