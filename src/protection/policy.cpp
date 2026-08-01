#include "auto_cattery/protection/policy.hpp"

#include <algorithm>
#include <array>

namespace autocattery::protection {
namespace {

struct Permissions {
    bool managed;
    bool cull;
    bool move;
};

constexpr Permissions PermissionsFor(ProtectionLevel level) noexcept {
    switch (level) {
    case ProtectionLevel::None:
        return {true, true, true};
    case ProtectionLevel::NoCull:
        return {true, false, true};
    case ProtectionLevel::NoMove:
        return {true, true, false};
    case ProtectionLevel::NoCullOrMove:
        return {true, false, false};
    case ProtectionLevel::FullyUnmanaged:
        return {false, false, false};
    }
    return {false, false, false};
}

constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

void HashByte(std::uint64_t& hash, std::uint8_t value) noexcept {
    hash ^= value;
    hash *= kFnvPrime;
}

void HashU64(std::uint64_t& hash, std::uint64_t value) noexcept {
    for (unsigned shift = 0; shift < 64; shift += 8) {
        HashByte(hash, static_cast<std::uint8_t>(value >> shift));
    }
}

}  // namespace

ProtectionLevel Merge(
    ProtectionLevel left,
    ProtectionLevel right) noexcept {
    if (left == ProtectionLevel::FullyUnmanaged ||
        right == ProtectionLevel::FullyUnmanaged) {
        return ProtectionLevel::FullyUnmanaged;
    }
    const auto left_permissions = PermissionsFor(left);
    const auto right_permissions = PermissionsFor(right);
    const bool cull =
        left_permissions.cull && right_permissions.cull;
    const bool move =
        left_permissions.move && right_permissions.move;
    if (!cull && !move) {
        return ProtectionLevel::NoCullOrMove;
    }
    if (!cull) {
        return ProtectionLevel::NoCull;
    }
    if (!move) {
        return ProtectionLevel::NoMove;
    }
    return ProtectionLevel::None;
}

ProtectionDecision Evaluate(
    const ProtectionInput& input,
    std::optional<std::int64_t> current_game_day) {
    ProtectionDecision decision;
    decision.cat_id = input.cat_id;
    decision.blacklist_preferred = input.blacklist_preferred;

    const auto add_hard_protection = [&](
        ProtectionLevel level,
        ProtectionSource source,
        const char* reason) {
        decision.effective_level =
            Merge(decision.effective_level, level);
        decision.sources.push_back(source);
        decision.reasons.push_back(reason);
    };
    const auto fail_closed = [&](const char* reason) {
        decision.fail_closed = true;
        add_hard_protection(
            ProtectionLevel::NoCullOrMove,
            ProtectionSource::ConservativeUnknown,
            reason);
    };

    if (!input.source_boundary_valid) {
        fail_closed("protection source boundary is unavailable or invalid");
    }
    if (input.native.locked == snapshot::TriState::Yes) {
        add_hard_protection(
            ProtectionLevel::NoCullOrMove,
            ProtectionSource::GameNative,
            "game-native lock is confirmed");
    } else if (input.native.locked == snapshot::TriState::Unknown) {
        fail_closed("game-native lock state is unknown");
    }
    if (input.native.favorite == snapshot::TriState::Yes) {
        add_hard_protection(
            ProtectionLevel::NoCull,
            ProtectionSource::GameNative,
            "game-native favorite is confirmed");
    } else if (input.native.favorite == snapshot::TriState::Unknown) {
        fail_closed("game-native favorite state is unknown");
    }
    if (input.native.special_state_present == snapshot::TriState::Yes) {
        add_hard_protection(
            ProtectionLevel::NoCullOrMove,
            ProtectionSource::GameNative,
            "game-native special state is confirmed");
    } else if (
        input.native.special_state_present == snapshot::TriState::Unknown) {
        fail_closed("game-native special state is unknown");
    }

    if (input.sidecar_record.has_value()) {
        const auto& record = *input.sidecar_record;
        const bool expired =
            record.expires_on_day.has_value() &&
            current_game_day.has_value() &&
            *record.expires_on_day < *current_game_day;
        if (record.cat_id != input.cat_id || input.identity_conflict) {
            fail_closed("sidecar identity conflicts with the snapshot cat");
        } else if (!input.stable_identity_confirmed) {
            fail_closed("stable identity is required for sidecar records");
        } else if (!expired) {
            decision.fixed_room = input.fixed_room;
            add_hard_protection(
                record.level,
                ProtectionSource::ModSidecar,
                "MOD sidecar protection applied");
        }
    }

    const auto permissions = PermissionsFor(decision.effective_level);
    decision.automatically_managed = permissions.managed;
    decision.cull_allowed = permissions.cull;
    decision.move_allowed = permissions.move;
    return decision;
}

ProtectionDigest BuildDigest(
    std::span<const ProtectionDecision> decisions) noexcept {
    std::vector<const ProtectionDecision*> ordered;
    ordered.reserve(decisions.size());
    for (const auto& decision : decisions) {
        ordered.push_back(&decision);
    }
    std::sort(
        ordered.begin(),
        ordered.end(),
        [](const auto* left, const auto* right) {
            return left->cat_id < right->cat_id;
        });

    ProtectionDigest digest;
    digest.value = kFnvOffset;
    for (const auto* decision : ordered) {
        digest.entries.push_back({
            decision->cat_id,
            decision->effective_level,
            decision->automatically_managed,
            decision->cull_allowed,
            decision->move_allowed,
            decision->fail_closed,
            decision->fixed_room
        });
        HashU64(
            digest.value,
            static_cast<std::uint64_t>(decision->cat_id));
        HashByte(
            digest.value,
            static_cast<std::uint8_t>(decision->effective_level));
        HashByte(digest.value, decision->automatically_managed);
        HashByte(digest.value, decision->cull_allowed);
        HashByte(digest.value, decision->move_allowed);
        HashByte(digest.value, decision->fail_closed);
        HashByte(digest.value, decision->fixed_room.has_value());
        if (decision->fixed_room) {
            HashU64(digest.value, decision->fixed_room->size());
            for (const unsigned char byte : *decision->fixed_room) {
                HashByte(digest.value, byte);
            }
        }
        if (!decision->cull_allowed || !decision->move_allowed ||
            !decision->automatically_managed) {
            ++digest.protected_count;
        }
    }
    return digest;
}

RecheckResult Recheck(
    const ProtectionDigest& preview,
    const ProtectionDigest& current) noexcept {
    return preview == current
        ? RecheckResult::Unchanged
        : RecheckResult::CancelAndRepreview;
}

}  // namespace autocattery::protection
