#include "auto_cattery/classification/classifier.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace autocattery::classification {
namespace {

template<class ResultType>
std::vector<snapshot::CatId> EligibleIds(
    const std::vector<ResultType>& ranked) {
    std::vector<snapshot::CatId> ids;
    for (const auto& result : ranked) {
        if (result.eligible) {
            ids.push_back(result.cat_id);
        }
    }
    return ids;
}

std::unordered_set<snapshot::CatId> Take(
    const std::vector<snapshot::CatId>& ids,
    std::size_t count) {
    const auto bounded = std::min(count, ids.size());
    return {ids.begin(), ids.begin() + static_cast<std::ptrdiff_t>(bounded)};
}

template<class ResultType>
std::unordered_map<snapshot::CatId, const ResultType*> ById(
    const std::vector<ResultType>& results) {
    std::unordered_map<snapshot::CatId, const ResultType*> by_id;
    for (const auto& result : results) {
        by_id.emplace(result.cat_id, &result);
    }
    return by_id;
}

}  // namespace

Result<void> Validate(const ClassificationConfig& config) {
    if (config.version != 1) {
        return {ErrorCode::ConfigInvalid, "unsupported classification version"};
    }
    if (!std::isfinite(config.never_cull_if_data_confidence_below) ||
        config.never_cull_if_data_confidence_below < 0.0 ||
        config.never_cull_if_data_confidence_below > 1.0) {
        return {
            ErrorCode::ConfigInvalid,
            "cull confidence threshold must be finite and between zero and one"
        };
    }
    return {};
}

Result<ClassificationPlan> ClassifyCats(
    const snapshot::HouseSnapshot& snapshot,
    const scoring::CombatRanking& combat,
    const breeding::BreedingRanking& breeding,
    const breeding::BreedingScoringConfig& breeding_config,
    const ClassificationConfig& config,
    const CullSafetyFactsByCat& safety_facts) {
    const auto config_validation = Validate(config);
    if (!config_validation) {
        return {{}, config_validation.code, config_validation.message};
    }
    if (!snapshot::Validate(snapshot).Valid() ||
        combat.source_snapshot_id != snapshot.snapshot_id ||
        breeding.source_snapshot_id != snapshot.snapshot_id) {
        return {
            {},
            ErrorCode::SnapshotInvalid,
            "classification inputs must reference the same valid snapshot"
        };
    }

    const auto combat_by_id = ById(combat.ranked);
    const auto breeding_by_id = ById(breeding.ranked);
    if (combat_by_id.size() != snapshot.cats.size() ||
        breeding_by_id.size() != snapshot.cats.size()) {
        return {
            {},
            ErrorCode::SnapshotInvalid,
            "rankings must contain exactly one result for every cat"
        };
    }
    for (const auto& cat : snapshot.cats) {
        if (!combat_by_id.contains(cat.id) ||
            !breeding_by_id.contains(cat.id)) {
            return {
                {},
                ErrorCode::SnapshotInvalid,
                "ranking result is missing a snapshot cat"
            };
        }
    }

    const auto combat_eligible = EligibleIds(combat.ranked);
    const auto breeding_eligible = EligibleIds(breeding.ranked);
    const std::unordered_set<snapshot::CatId> combat_recommended{
        combat.recommended_cat_ids.begin(),
        combat.recommended_cat_ids.end()
    };
    if (combat_recommended.size() != combat.recommended_cat_ids.size() ||
        std::ranges::any_of(
            combat_recommended,
            [&](const auto id) { return !combat_by_id.contains(id); })) {
        return {
            {},
            ErrorCode::SnapshotInvalid,
            "recommended combat IDs must be unique snapshot cats"
        };
    }
    const auto combat_pool = Take(
        combat_eligible,
        std::max(
            combat.recommended_cat_ids.size(),
            config.minimum_combat_pool));
    auto breeding_core =
        Take(breeding_eligible, breeding_config.core_breeders);
    std::unordered_map<snapshot::CatId, snapshot::CatId>
        breeding_partners;
    if (breeding.recommended_pair) {
        const auto& pair = *breeding.recommended_pair;
        breeding_core.insert(pair.cat_a_id);
        breeding_core.insert(pair.cat_b_id);
        breeding_partners.emplace(pair.cat_a_id, pair.cat_b_id);
        breeding_partners.emplace(pair.cat_b_id, pair.cat_a_id);
    }

    std::unordered_set<snapshot::CatId> breeding_reserve;
    const auto reserve_begin =
        std::min(breeding_config.core_breeders, breeding_eligible.size());
    const auto reserve_end = breeding_config.reserve_breeders >
            breeding_eligible.size() - reserve_begin
        ? breeding_eligible.size()
        : reserve_begin + breeding_config.reserve_breeders;
    breeding_reserve.insert(
        breeding_eligible.begin() +
            static_cast<std::ptrdiff_t>(reserve_begin),
        breeding_eligible.begin() +
            static_cast<std::ptrdiff_t>(reserve_end));
    const auto configured_breeding_pool =
        breeding_config.reserve_breeders >
            std::numeric_limits<std::size_t>::max() -
                breeding_config.core_breeders
        ? std::numeric_limits<std::size_t>::max()
        : breeding_config.core_breeders +
            breeding_config.reserve_breeders;
    const auto breeding_pool = Take(
        breeding_eligible,
        std::max(
            configured_breeding_pool,
            config.minimum_breeding_pool));

    ClassificationPlan plan;
    plan.source_snapshot_id = snapshot.snapshot_id;
    plan.algorithm_version = kClassificationAlgorithmVersion;
    const bool combat_pool_satisfied =
        combat_eligible.size() >= config.minimum_combat_pool;
    const bool breeding_pool_satisfied =
        breeding_eligible.size() >= config.minimum_breeding_pool;
    if (!combat_pool_satisfied) {
        plan.global_warnings.push_back(
            "minimum confirmed combat pool is not satisfied");
    }
    if (!breeding_pool_satisfied) {
        plan.global_warnings.push_back(
            "minimum confirmed breeding pool is not satisfied");
    }
    if (!snapshot.capabilities.stable_cat_id) {
        plan.global_warnings.push_back(
            "stable cat identity is not confirmed");
    }
    if (!snapshot.capabilities.read_breeding_eligibility) {
        plan.global_warnings.push_back(
            "breeding eligibility is not confirmed by the adapter");
    }
    if (!snapshot.capabilities.read_relationships) {
        plan.global_warnings.push_back(
            "relationship safeguards are not available");
    }
    plan.global_warnings.push_back(
        "capacity is not evaluated until stage 09; relief entries are only "
        "an ordered safe candidate pool");

    std::vector<snapshot::CatId> unresolved;
    plan.decisions.reserve(snapshot.cats.size());
    for (const auto& cat : snapshot.cats) {
        const auto& combat_score = *combat_by_id.at(cat.id);
        const auto& breeding_score = *breeding_by_id.at(cat.id);
        const auto facts = safety_facts.find(cat.id);
        const auto* policy =
            facts != safety_facts.end() &&
                facts->second.policy_decision.has_value()
            ? &*facts->second.policy_decision
            : nullptr;

        CatDecision decision;
        decision.cat_id = cat.id;
        decision.combat_score = combat_score.score;
        decision.breeding_score = breeding_score.score;
        decision.confidence =
            std::min(combat_score.confidence, breeding_score.confidence);
        decision.combat_recommended = combat_recommended.contains(cat.id);
        decision.combat_pool_protected = combat_pool.contains(cat.id);
        decision.breeding_core = breeding_core.contains(cat.id);
        if (const auto partner = breeding_partners.find(cat.id);
            partner != breeding_partners.end()) {
            decision.breeding_partner_id = partner->second;
            decision.breeding_stats_stable =
                breeding.stage == breeding::BreedingStage::StableAllSeven;
        }
        decision.breeding_reserve = breeding_reserve.contains(cat.id);
        decision.breeding_pool_protected = breeding_pool.contains(cat.id);
        if (policy != nullptr) {
            decision.protection_level = policy->effective_level;
            decision.move_allowed = policy->move_allowed;
            decision.blacklist_preferred = policy->blacklist_preferred;
        }

        if (policy != nullptr &&
            !policy->automatically_managed) {
            decision.primary_role = CatRole::ProtectedUnmanaged;
            decision.reasons.push_back(
                "protection policy excludes this cat from automatic "
                "classification");
        } else if (!combat_score.eligible && !breeding_score.eligible) {
            decision.primary_role = CatRole::Ineligible;
            decision.reasons.push_back(
                "not eligible for confirmed combat or breeding pools");
        } else if (decision.combat_recommended &&
                   decision.breeding_core) {
            decision.primary_role =
                config.combat_priority_over_breeding
                    ? CatRole::CombatRecommended
                    : CatRole::BreedingCore;
        } else if (decision.breeding_core) {
            decision.primary_role = CatRole::BreedingCore;
        } else if (decision.combat_recommended) {
            decision.primary_role = CatRole::CombatRecommended;
        } else if (decision.breeding_reserve) {
            decision.primary_role = CatRole::BreedingReserve;
        } else {
            decision.primary_role = CatRole::GeneralReserve;
            unresolved.push_back(cat.id);
        }
        plan.decisions.push_back(std::move(decision));
    }

    std::sort(
        unresolved.begin(),
        unresolved.end(),
        [&](const auto left, const auto right) {
            const auto left_value =
                combat_by_id.at(left)->score + breeding_by_id.at(left)->score;
            const auto right_value =
                combat_by_id.at(right)->score + breeding_by_id.at(right)->score;
            if (left_value != right_value) {
                return left_value > right_value;
            }
            return left < right;
        });
    const auto general_reserve =
        Take(unresolved, config.minimum_general_reserve);

    std::unordered_map<snapshot::CatId, const snapshot::CatSnapshot*> cats;
    for (const auto& cat : snapshot.cats) {
        cats.emplace(cat.id, &cat);
    }
    for (auto& decision : plan.decisions) {
        if (decision.primary_role != CatRole::GeneralReserve) {
            continue;
        }

        const auto& cat = *cats.at(decision.cat_id);
        const auto facts = safety_facts.find(decision.cat_id);
        bool guarded{};
        const auto guard = [&](bool blocked, const char* reason) {
            if (blocked) {
                guarded = true;
                decision.reasons.push_back(reason);
            }
        };
        guard(
            general_reserve.contains(decision.cat_id),
            "minimum general reserve");
        guard(
            !combat_pool_satisfied || !breeding_pool_satisfied,
            "minimum combat or breeding pool is not confirmed");
        guard(
            !snapshot.capabilities.stable_cat_id,
            "stable cat identity is not confirmed");
        guard(
            !snapshot.capabilities.read_breeding_eligibility,
            "breeding eligibility capability is unavailable");
        guard(
            !snapshot.capabilities.read_relationships,
            "relationship safeguards are unavailable");
        guard(
            facts == safety_facts.end() ||
                !facts->second.policy_decision.has_value(),
            "protection policy decision is unavailable");
        guard(
            facts == safety_facts.end() ||
                facts->second.protected_from_cull ==
                    snapshot::TriState::Unknown,
            "cull protection is unconfirmed");
        guard(
            facts != safety_facts.end() &&
                facts->second.protected_from_cull ==
                    snapshot::TriState::Yes,
            "confirmed protected from cull");
        guard(
            facts != safety_facts.end() &&
                facts->second.policy_decision.has_value() &&
                !facts->second.policy_decision->cull_allowed,
            "protection policy forbids culling");
        guard(
            facts == safety_facts.end() ||
                facts->second.special_state_present ==
                    snapshot::TriState::Unknown,
            "special state is unconfirmed");
        guard(
            facts != safety_facts.end() &&
                facts->second.special_state_present ==
                    snapshot::TriState::Yes,
            "confirmed special state");
        guard(
            cat.life_stage != snapshot::LifeStage::Adult,
            "confirmed adult life stage is required");
        guard(
            cat.available_for_combat == snapshot::TriState::Unknown ||
                cat.available_for_breeding == snapshot::TriState::Unknown ||
                cat.injured == snapshot::TriState::Unknown,
            "combat, breeding, or injury status is unconfirmed");
        guard(cat.in_adventure_box, "cat is in the adventure box");
        guard(
            decision.confidence <
                config.never_cull_if_data_confidence_below,
            "data confidence is below the cull threshold");

        if (!guarded) {
            decision.primary_role = CatRole::CullCandidate;
            decision.preview_cull_candidate = true;
            decision.destructive_action_allowed = false;
            decision.reasons.push_back(
                "outside retained pools with all stage-07 preview guards "
                "confirmed");
            plan.quality_cull_candidates.push_back(decision.cat_id);
        }
    }

    std::sort(
        plan.quality_cull_candidates.begin(),
        plan.quality_cull_candidates.end(),
        [&](const auto left, const auto right) {
            const auto left_facts = safety_facts.find(left);
            const auto right_facts = safety_facts.find(right);
            const bool left_blacklisted =
                left_facts != safety_facts.end() &&
                left_facts->second.policy_decision.has_value() &&
                left_facts->second.policy_decision->blacklist_preferred;
            const bool right_blacklisted =
                right_facts != safety_facts.end() &&
                right_facts->second.policy_decision.has_value() &&
                right_facts->second.policy_decision->blacklist_preferred;
            if (left_blacklisted != right_blacklisted) {
                return left_blacklisted;
            }
            const auto left_value =
                combat_by_id.at(left)->score + breeding_by_id.at(left)->score;
            const auto right_value =
                combat_by_id.at(right)->score + breeding_by_id.at(right)->score;
            if (left_value != right_value) {
                return left_value < right_value;
            }
            return left < right;
        });
    plan.capacity_relief_candidates = plan.quality_cull_candidates;
    return {std::move(plan)};
}

}  // namespace autocattery::classification
