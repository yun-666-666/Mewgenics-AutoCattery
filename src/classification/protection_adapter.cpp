#include "auto_cattery/classification/protection_adapter.hpp"

namespace autocattery::classification {

CullSafetyFactsByCat BuildCullSafetyFacts(
    const snapshot::HouseSnapshot& snapshot,
    const NativeProtectionFactsByCat& native_facts,
    const protection::ProtectionSidecar& sidecar,
    const IdentityTokenByCat& identity_tokens) {
    CullSafetyFactsByCat facts_by_cat;
    facts_by_cat.reserve(snapshot.cats.size());
    for (const auto& cat : snapshot.cats) {
        CullSafetyFacts facts;
        const auto native = native_facts.find(cat.id);
        if (native != native_facts.end()) {
            facts.special_state_present =
                native->second.special_state_present;
        }

        protection::ProtectionInput input;
        input.cat_id = cat.id;
        if (native != native_facts.end()) {
            input.native = native->second;
        }
        input.source_boundary_valid =
            sidecar.status == protection::SidecarLoadStatus::Loaded &&
            !sidecar.destructive_actions_blocked;
        input.blacklist_preferred = sidecar.blacklist.contains(cat.id);

        const auto identity = identity_tokens.find(cat.id);
        const protection::SidecarRecord* matched{};
        bool ambiguous{};
        for (const auto& candidate : sidecar.records) {
            if (candidate.protection.cat_id != cat.id ||
                (candidate.source_save_name &&
                 *candidate.source_save_name != snapshot.source_save_name) ||
                (!candidate.identity_token.empty() &&
                 (identity == identity_tokens.end() ||
                  identity->second != candidate.identity_token))) {
                continue;
            }
            ambiguous = matched != nullptr;
            if (matched == nullptr) {
                matched = &candidate;
            }
        }
        if (matched != nullptr) {
            input.sidecar_record = matched->protection;
            input.fixed_room = matched->fixed_room;
            input.stable_identity_confirmed =
                snapshot.capabilities.stable_cat_id &&
                (matched->identity_token.empty() ||
                 identity != identity_tokens.end());
            input.identity_conflict = ambiguous;
        } else {
            input.stable_identity_confirmed =
                snapshot.capabilities.stable_cat_id;
        }
        facts.policy_decision =
            protection::Evaluate(input, snapshot.game_day);
        facts.protected_from_cull =
            facts.policy_decision->cull_allowed
                ? snapshot::TriState::No
                : snapshot::TriState::Yes;
        facts_by_cat.emplace(cat.id, std::move(facts));
    }
    return facts_by_cat;
}

}  // namespace autocattery::classification
