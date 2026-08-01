#include "auto_cattery/protection/sidecar.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>

#include <nlohmann/json.hpp>

namespace autocattery::protection {
namespace {

using Json = nlohmann::json;

ProtectionSidecar Failed(
    SidecarLoadStatus status,
    std::string limitation) {
    ProtectionSidecar result;
    result.status = status;
    result.destructive_actions_blocked = true;
    result.limitation = std::move(limitation);
    return result;
}

std::optional<ProtectionLevel> ParseLevel(std::string_view value) {
    if (value == "None") {
        return ProtectionLevel::None;
    }
    if (value == "NoCull") {
        return ProtectionLevel::NoCull;
    }
    if (value == "NoMove") {
        return ProtectionLevel::NoMove;
    }
    if (value == "NoCullOrMove") {
        return ProtectionLevel::NoCullOrMove;
    }
    if (value == "FullyUnmanaged") {
        return ProtectionLevel::FullyUnmanaged;
    }
    return std::nullopt;
}

bool HasOnlyKeys(
    const Json& object,
    std::initializer_list<std::string_view> allowed) {
    for (const auto& [key, value] : object.items()) {
        static_cast<void>(value);
        bool found{};
        for (const auto candidate : allowed) {
            if (key == candidate) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

bool ValidCatId(const Json& value) {
    if (value.is_number_unsigned()) {
        const auto raw = value.get<std::uint64_t>();
        return raw > 0 &&
            raw <= static_cast<std::uint64_t>(
                std::numeric_limits<snapshot::CatId>::max());
    }
    return value.is_number_integer() &&
        value.get<std::int64_t>() > 0;
}

}  // namespace

ProtectionSidecar ParseProtectionSidecar(std::string_view contents) {
    if (contents.empty() ||
        std::ranges::all_of(
            contents,
            [](unsigned char value) { return std::isspace(value) != 0; })) {
        return Failed(SidecarLoadStatus::Empty, "sidecar is empty");
    }

    Json root;
    try {
        root = Json::parse(contents);
    } catch (const Json::exception&) {
        return Failed(
            SidecarLoadStatus::InvalidJson,
            "sidecar JSON is malformed");
    }
    if (!root.is_object() ||
        !HasOnlyKeys(root, {"schema_version", "records", "blacklist"}) ||
        !root.contains("schema_version") ||
        !root["schema_version"].is_number_unsigned()) {
        return Failed(
            SidecarLoadStatus::InvalidSchema,
            "sidecar root does not match the strict schema");
    }

    const auto schema = root["schema_version"].get<std::uint64_t>();
    if (schema < kProtectionSidecarSchemaVersion) {
        return Failed(
            SidecarLoadStatus::OldSchema,
            "older sidecar schema requires an explicit migration");
    }
    if (schema > kProtectionSidecarSchemaVersion) {
        return Failed(
            SidecarLoadStatus::FutureSchema,
            "future sidecar schema cannot be interpreted safely");
    }
    if (!root.contains("records") || !root["records"].is_array() ||
        !root.contains("blacklist") || !root["blacklist"].is_array()) {
        return Failed(
            SidecarLoadStatus::InvalidSchema,
            "sidecar records and blacklist arrays are required");
    }

    ProtectionSidecar result;
    result.status = SidecarLoadStatus::Loaded;
    for (const auto& entry : root["records"]) {
        if (!entry.is_object() ||
            !HasOnlyKeys(
                entry,
                {"cat_id", "level", "identity_token", "reason",
                 "expires_on_day", "fixed_room", "source_save_name",
                 "display_name"}) ||
            !entry.contains("cat_id") || !ValidCatId(entry["cat_id"]) ||
            !entry.contains("level") || !entry["level"].is_string() ||
            (entry.contains("identity_token") &&
             !entry["identity_token"].is_string())) {
            return Failed(
                SidecarLoadStatus::InvalidSchema,
                "sidecar protection record is invalid");
        }
        const auto cat_id = entry["cat_id"].get<snapshot::CatId>();
        const auto level =
            ParseLevel(entry["level"].get_ref<const std::string&>());
        const auto identity = entry.contains("identity_token")
            ? entry["identity_token"].get<std::string>()
            : std::string{};
        const bool duplicate = std::ranges::any_of(
            result.records,
            [&](const auto& existing) {
                if (!identity.empty()) {
                    return existing.identity_token == identity;
                }
                return existing.protection.cat_id == cat_id &&
                    existing.identity_token.empty();
            });
        if (!level.has_value() || identity.size() > 256 || duplicate) {
            return Failed(
                SidecarLoadStatus::InvalidSchema,
                "sidecar has an invalid level or duplicate identity");
        }

        SidecarRecord record;
        record.protection.cat_id = cat_id;
        record.protection.level = *level;
        record.identity_token = identity;
        if (entry.contains("display_name")) {
            if (!entry["display_name"].is_string() ||
                entry["display_name"].get_ref<const std::string&>().size() >
                    256) {
                return Failed(
                    SidecarLoadStatus::InvalidSchema,
                    "sidecar display name is invalid");
            }
            record.display_name = entry["display_name"].get<std::string>();
        }
        if (entry.contains("fixed_room")) {
            if (!entry["fixed_room"].is_string()) {
                return Failed(
                    SidecarLoadStatus::InvalidSchema,
                    "sidecar fixed room is invalid");
            }
            const auto room = entry["fixed_room"].get<std::string>();
            if (room.empty() || room.size() > 128) {
                return Failed(
                    SidecarLoadStatus::InvalidSchema,
                    "sidecar fixed room is invalid");
            }
            record.fixed_room = room;
        }
        if (entry.contains("source_save_name")) {
            if (!entry["source_save_name"].is_string()) {
                return Failed(
                    SidecarLoadStatus::InvalidSchema,
                    "sidecar source save name is invalid");
            }
            const auto name = entry["source_save_name"].get<std::string>();
            if (name.empty() || name.size() > 128 ||
                name.find('/') != std::string::npos ||
                name.find('\\') != std::string::npos) {
                return Failed(
                    SidecarLoadStatus::InvalidSchema,
                    "sidecar source save name is invalid");
            }
            record.source_save_name = name;
        }
        if (entry.contains("reason")) {
            if (!entry["reason"].is_string() ||
                entry["reason"].get_ref<const std::string&>().size() > 1024) {
                return Failed(
                    SidecarLoadStatus::InvalidSchema,
                    "sidecar reason is invalid");
            }
            record.protection.reason =
                entry["reason"].get<std::string>();
        }
        if (entry.contains("expires_on_day")) {
            if (!entry["expires_on_day"].is_number_integer()) {
                return Failed(
                    SidecarLoadStatus::InvalidSchema,
                    "sidecar expiry must be an integer game day");
            }
            constexpr std::int64_t kMaximumGameDay = 1'000'000'000;
            std::int64_t expiry{};
            if (entry["expires_on_day"].is_number_unsigned()) {
                const auto raw =
                    entry["expires_on_day"].get<std::uint64_t>();
                if (raw > static_cast<std::uint64_t>(kMaximumGameDay)) {
                    return Failed(
                        SidecarLoadStatus::InvalidSchema,
                        "sidecar expiry is outside the accepted range");
                }
                expiry = static_cast<std::int64_t>(raw);
            } else {
                expiry =
                    entry["expires_on_day"].get<std::int64_t>();
            }
            if (expiry < 0 || expiry > kMaximumGameDay) {
                return Failed(
                    SidecarLoadStatus::InvalidSchema,
                    "sidecar expiry is outside the accepted range");
            }
            record.protection.expires_on_day = expiry;
        }
        result.records.push_back(std::move(record));
    }

    for (const auto& entry : root["blacklist"]) {
        if (!ValidCatId(entry)) {
            return Failed(
                SidecarLoadStatus::InvalidSchema,
                "sidecar blacklist CatId is invalid");
        }
        const auto cat_id = entry.get<snapshot::CatId>();
        if (!result.blacklist.insert(cat_id).second) {
            return Failed(
                SidecarLoadStatus::InvalidSchema,
                "sidecar blacklist contains a duplicate CatId");
        }
    }

    result.destructive_actions_blocked = false;
    return result;
}

ProtectionSidecar LoadProtectionSidecar(
    const std::filesystem::path& path) {
    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        return Failed(
            error ? SidecarLoadStatus::ReadFailed
                  : SidecarLoadStatus::Missing,
            error ? "sidecar existence check failed"
                  : "sidecar is missing");
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return Failed(
            SidecarLoadStatus::ReadFailed,
            "sidecar could not be opened");
    }
    std::ostringstream contents;
    contents << stream.rdbuf();
    if (stream.bad()) {
        return Failed(
            SidecarLoadStatus::ReadFailed,
            "sidecar read failed");
    }
    return ParseProtectionSidecar(contents.str());
}

}  // namespace autocattery::protection
