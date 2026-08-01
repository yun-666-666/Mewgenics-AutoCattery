#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "auto_cattery/error.hpp"
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
    std::string display_name;
    std::optional<snapshot::RoomId> fixed_room;
    std::optional<std::string> source_save_name;
};

struct ProtectionSidecar {
    SidecarLoadStatus status{SidecarLoadStatus::Missing};
    bool destructive_actions_blocked{true};
    std::vector<SidecarRecord> records;
    std::unordered_set<snapshot::CatId> blacklist;
    std::string limitation;
};

[[nodiscard]] ProtectionSidecar ParseProtectionSidecar(
    std::string_view contents);

[[nodiscard]] ProtectionSidecar LoadProtectionSidecar(
    const std::filesystem::path& path);

[[nodiscard]] Result<void> SaveProtectionSidecar(
    const std::filesystem::path& path,
    const ProtectionSidecar& sidecar);

void UpsertProtectionRecord(
    ProtectionSidecar& sidecar,
    SidecarRecord record);

bool RemoveProtectionRecord(
    ProtectionSidecar& sidecar,
    std::string_view identity_token);

}  // namespace autocattery::protection
