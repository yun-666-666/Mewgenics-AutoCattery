#include "auto_cattery/protection/policy.hpp"

#include <algorithm>
#include <array>

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

using protection::ProtectionLevel;

protection::ProtectionInput KnownInput(snapshot::CatId id = 1) {
    protection::ProtectionInput input;
    input.cat_id = id;
    input.native.locked = snapshot::TriState::No;
    input.native.favorite = snapshot::TriState::No;
    input.native.special_state_present = snapshot::TriState::No;
    input.stable_identity_confirmed = true;
    return input;
}

void CheckLevel(
    ProtectionLevel level,
    bool managed,
    bool can_cull,
    bool can_move) {
    auto input = KnownInput();
    input.sidecar_record = protection::ProtectionRecord{
        input.cat_id, level, "test", std::nullopt};
    const auto decision = protection::Evaluate(input);
    AC_CHECK(decision.effective_level == level);
    AC_CHECK(decision.automatically_managed == managed);
    AC_CHECK(decision.cull_allowed == can_cull);
    AC_CHECK(decision.move_allowed == can_move);
}

}  // namespace

void RunProtectionPolicyTests() {
    CheckLevel(ProtectionLevel::None, true, true, true);
    CheckLevel(ProtectionLevel::NoCull, true, false, true);
    CheckLevel(ProtectionLevel::NoMove, true, true, false);
    CheckLevel(ProtectionLevel::NoCullOrMove, true, false, false);
    CheckLevel(ProtectionLevel::FullyUnmanaged, false, false, false);

    AC_CHECK(
        protection::Merge(
            ProtectionLevel::NoCull,
            ProtectionLevel::NoMove) ==
        ProtectionLevel::NoCullOrMove);
    AC_CHECK(
        protection::Merge(
            ProtectionLevel::FullyUnmanaged,
            ProtectionLevel::None) ==
        ProtectionLevel::FullyUnmanaged);

    auto native_unknown = KnownInput();
    native_unknown.native.locked = snapshot::TriState::Unknown;
    const auto unknown_decision = protection::Evaluate(native_unknown);
    AC_CHECK(unknown_decision.fail_closed);
    AC_CHECK(!unknown_decision.cull_allowed);
    AC_CHECK(!unknown_decision.move_allowed);

    auto whitelist_and_blacklist = KnownInput();
    whitelist_and_blacklist.blacklist_preferred = true;
    whitelist_and_blacklist.sidecar_record = protection::ProtectionRecord{
        whitelist_and_blacklist.cat_id,
        ProtectionLevel::NoCull,
        "whitelist",
        std::nullopt};
    const auto conflict_decision =
        protection::Evaluate(whitelist_and_blacklist);
    AC_CHECK(conflict_decision.blacklist_preferred);
    AC_CHECK(!conflict_decision.cull_allowed);

    auto identity_conflict = whitelist_and_blacklist;
    identity_conflict.sidecar_record->level = ProtectionLevel::None;
    identity_conflict.identity_conflict = true;
    const auto identity_decision = protection::Evaluate(identity_conflict);
    AC_CHECK(identity_decision.fail_closed);
    AC_CHECK(!identity_decision.cull_allowed);

    auto unstable_identity = whitelist_and_blacklist;
    unstable_identity.sidecar_record->level = ProtectionLevel::None;
    unstable_identity.stable_identity_confirmed = false;
    const auto unstable_decision =
        protection::Evaluate(unstable_identity);
    AC_CHECK(unstable_decision.fail_closed);
    AC_CHECK(!unstable_decision.cull_allowed);

    auto expired = whitelist_and_blacklist;
    expired.sidecar_record->expires_on_day = 10;
    const auto expired_decision = protection::Evaluate(expired, 11);
    AC_CHECK(expired_decision.effective_level == ProtectionLevel::None);
    AC_CHECK(expired_decision.cull_allowed);

    std::array<protection::ProtectionDecision, 2> preview{
        protection::Evaluate(KnownInput(2)),
        conflict_decision};
    const auto digest_before = protection::BuildDigest(preview);
    std::ranges::reverse(preview);
    AC_CHECK(protection::BuildDigest(preview) == digest_before);
    preview[0].effective_level = ProtectionLevel::FullyUnmanaged;
    preview[0].automatically_managed = false;
    preview[0].move_allowed = false;
    const auto digest_after = protection::BuildDigest(preview);
    AC_CHECK(
        protection::Recheck(digest_before, digest_after) ==
        protection::RecheckResult::CancelAndRepreview);
    AC_CHECK(
        protection::Recheck(digest_before, digest_before) ==
        protection::RecheckResult::Unchanged);
}

}  // namespace autocattery::tests
