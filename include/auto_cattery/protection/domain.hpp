#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "auto_cattery/snapshot/domain.hpp"

namespace autocattery::protection {

enum class ProtectionLevel {
    None,
    NoCull,
    NoMove,
    NoCullOrMove,
    FullyUnmanaged
};

enum class ProtectionSource {
    GameNative,
    ModSidecar,
    ConservativeUnknown
};

struct ProtectionRecord {
    snapshot::CatId cat_id{};
    ProtectionLevel level{ProtectionLevel::None};
    std::string reason;
    std::optional<std::int64_t> expires_on_day;
};

struct NativeProtectionFacts {
    snapshot::TriState locked{snapshot::TriState::Unknown};
    snapshot::TriState favorite{snapshot::TriState::Unknown};
    snapshot::TriState special_state_present{snapshot::TriState::Unknown};
};

struct ProtectionInput {
    snapshot::CatId cat_id{};
    NativeProtectionFacts native;
    std::optional<ProtectionRecord> sidecar_record;
    bool blacklist_preferred{};
    bool stable_identity_confirmed{};
    bool identity_conflict{};
    bool source_boundary_valid{true};
    std::optional<snapshot::RoomId> fixed_room;
};

struct ProtectionDecision {
    snapshot::CatId cat_id{};
    ProtectionLevel effective_level{ProtectionLevel::None};
    bool automatically_managed{true};
    bool cull_allowed{};
    bool move_allowed{};
    bool blacklist_preferred{};
    bool fail_closed{};
    std::optional<snapshot::RoomId> fixed_room;
    std::vector<ProtectionSource> sources;
    std::vector<std::string> reasons;
};

struct ProtectionDigestEntry {
    snapshot::CatId cat_id{};
    ProtectionLevel level{ProtectionLevel::NoCullOrMove};
    bool automatically_managed{};
    bool cull_allowed{};
    bool move_allowed{};
    bool fail_closed{};
    std::optional<snapshot::RoomId> fixed_room;

    bool operator==(const ProtectionDigestEntry&) const = default;
};

struct ProtectionDigest {
    std::uint64_t value{};
    std::size_t protected_count{};
    std::vector<ProtectionDigestEntry> entries;

    bool operator==(const ProtectionDigest&) const = default;
};

enum class RecheckResult {
    Unchanged,
    CancelAndRepreview
};

}  // namespace autocattery::protection
