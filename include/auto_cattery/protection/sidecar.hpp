#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "auto_cattery/protection/domain.hpp"

namespace autocattery::protection {

inline constexpr std::uint32_t kProtectionSidecarSchemaVersion = 1;

enum class SidecarLoadStatus {
    Loaded,
    Missing,
    Empty,
    ReadFailed,
    InvalidJson,
    InvalidSchema,
    OldSchema,
    FutureSchema
};

struct SidecarRecord {
    ProtectionRecord protection;
    std::string identity_token;
};

struct ProtectionSidecar {
    SidecarLoadStatus status{SidecarLoadStatus::Missing};
    bool destructive_actions_blocked{true};
    std::unordered_map<snapshot::CatId, SidecarRecord> records;
    std::unordered_set<snapshot::CatId> blacklist;
    std::string limitation;
};

[[nodiscard]] ProtectionSidecar ParseProtectionSidecar(
    std::string_view contents);

[[nodiscard]] ProtectionSidecar LoadProtectionSidecar(
    const std::filesystem::path& path);

}  // namespace autocattery::protection
