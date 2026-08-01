#include "move_only_protection.hpp"

#include "auto_cattery/classification/protection_adapter.hpp"
#include "auto_cattery/protection/identity.hpp"
#include "auto_cattery/protection/policy.hpp"
#include "auto_cattery/protection/sidecar.hpp"

namespace autocattery::workflow::detail {
namespace {

protection::ProtectionSidecar LoadSidecar(
    const std::filesystem::path& path) {
    const auto empty = [] {
        return protection::ParseProtectionSidecar(
            R"({"schema_version":1,"records":[],"blacklist":[]})");
    };
    if (path.empty()) {
        return empty();
    }
    auto sidecar = protection::LoadProtectionSidecar(path);
    return sidecar.status == protection::SidecarLoadStatus::Missing
        ? empty() : sidecar;
}

}  // namespace

MoveOnlyProtectionSet BuildMoveOnlyProtections(
    const snapshot::HouseSnapshot& snapshot,
    const std::filesystem::path& sidecar_path) {
    classification::NativeProtectionFactsByCat native;
    classification::IdentityTokenByCat identities;
    for (const auto& cat : snapshot.cats) {
        native[cat.id] = {
            snapshot::TriState::No,
            snapshot::TriState::No,
            snapshot::TriState::No
        };
        identities[cat.id] = protection::StableCatIdentityToken(cat);
    }
    MoveOnlyProtectionSet result;
    result.safety = classification::BuildCullSafetyFacts(
        snapshot, native, LoadSidecar(sidecar_path), identities);
    result.decisions.reserve(snapshot.cats.size());
    for (const auto& cat : snapshot.cats) {
        auto& decision = *result.safety[cat.id].policy_decision;
        decision.effective_level = protection::Merge(
            decision.effective_level,
            protection::ProtectionLevel::NoCull);
        decision.cull_allowed = false;
        result.safety[cat.id].protected_from_cull = snapshot::TriState::Yes;
        result.decisions.push_back(decision);
    }
    result.digest = protection::BuildDigest(result.decisions);
    return result;
}

}  // namespace autocattery::workflow::detail
