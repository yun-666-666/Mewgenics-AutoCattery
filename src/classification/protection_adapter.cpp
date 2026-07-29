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

        const auto record = sidecar.records.find(cat.id);
        const auto identity = identity_tokens.find(cat.id);
        if (record != sidecar.records.end()) {
            input.sidecar_record = record->second.protection;
            input.stable_identity_confirmed =
                snapshot.capabilities.stable_cat_id &&
                identity != identity_tokens.end();
            input.identity_conflict =
                identity != identity_tokens.end() &&
                identity->second != record->second.identity_token;
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
