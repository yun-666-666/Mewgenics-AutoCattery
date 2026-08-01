#include "auto_cattery/protection/sidecar.hpp"

#include <algorithm>
#include <fstream>
#include <system_error>
#include <tuple>

#include <nlohmann/json.hpp>
#include <windows.h>

namespace autocattery::protection {
namespace {

using Json = nlohmann::json;

const char* LevelName(ProtectionLevel level) {
    switch (level) {
        case ProtectionLevel::None: return "None";
        case ProtectionLevel::NoCull: return "NoCull";
        case ProtectionLevel::NoMove: return "NoMove";
        case ProtectionLevel::NoCullOrMove: return "NoCullOrMove";
        case ProtectionLevel::FullyUnmanaged: return "FullyUnmanaged";
    }
    return "FullyUnmanaged";
}

Json RecordJson(const SidecarRecord& record) {
    Json value{
        {"cat_id", record.protection.cat_id},
        {"level", LevelName(record.protection.level)},
        {"identity_token", record.identity_token}
    };
    if (!record.display_name.empty()) {
        value["display_name"] = record.display_name;
    }
    if (record.fixed_room) {
        value["fixed_room"] = *record.fixed_room;
    }
    if (record.source_save_name) {
        value["source_save_name"] = *record.source_save_name;
    }
    if (!record.protection.reason.empty()) {
        value["reason"] = record.protection.reason;
    }
    if (record.protection.expires_on_day) {
        value["expires_on_day"] = *record.protection.expires_on_day;
    }
    return value;
}

Result<void> WriteAtomic(
    const std::filesystem::path& path,
    const std::string& text) {
    if (path.empty()) {
        return {ErrorCode::ConfigInvalid, "protection path is empty"};
    }
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        return {ErrorCode::ConfigInvalid,
                "cannot create protection directory"};
    }
    auto temporary = path;
    temporary += L".candidate.tmp";
    std::filesystem::remove(temporary, error);
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output) {
        return {ErrorCode::ConfigInvalid,
                "cannot create protection candidate"};
    }
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    output.flush();
    if (!output) {
        return {ErrorCode::ConfigInvalid,
                "cannot write protection candidate"};
    }
    output.close();
    if (MoveFileExW(
            temporary.c_str(), path.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0) {
        std::filesystem::remove(temporary, error);
        return {ErrorCode::ConfigInvalid,
                "cannot atomically replace protection file"};
    }
    return {};
}

}  // namespace

Result<void> SaveProtectionSidecar(
    const std::filesystem::path& path,
    const ProtectionSidecar& sidecar) {
    auto records = sidecar.records;
    std::sort(
        records.begin(), records.end(),
        [](const auto& left, const auto& right) {
            return std::tie(left.protection.cat_id, left.identity_token) <
                std::tie(right.protection.cat_id, right.identity_token);
        });
    Json root{
        {"schema_version", kProtectionSidecarSchemaVersion},
        {"records", Json::array()},
        {"blacklist", Json::array()}
    };
    for (const auto& record : records) {
        root["records"].push_back(RecordJson(record));
    }
    std::vector<snapshot::CatId> blacklist(
        sidecar.blacklist.begin(), sidecar.blacklist.end());
    std::sort(blacklist.begin(), blacklist.end());
    root["blacklist"] = blacklist;
    const auto serialized = root.dump(2) + "\n";
    const auto parsed = ParseProtectionSidecar(serialized);
    if (parsed.status != SidecarLoadStatus::Loaded) {
        return {ErrorCode::ConfigInvalid,
                "protection candidate failed validation"};
    }
    return WriteAtomic(path, serialized);
}

void UpsertProtectionRecord(
    ProtectionSidecar& sidecar,
    SidecarRecord record) {
    const auto found = std::ranges::find(
        sidecar.records, record.identity_token,
        &SidecarRecord::identity_token);
    if (found == sidecar.records.end()) {
        sidecar.records.push_back(std::move(record));
    } else {
        *found = std::move(record);
    }
    sidecar.status = SidecarLoadStatus::Loaded;
    sidecar.destructive_actions_blocked = false;
}

bool RemoveProtectionRecord(
    ProtectionSidecar& sidecar,
    std::string_view identity_token) {
    const auto old_size = sidecar.records.size();
    std::erase_if(
        sidecar.records,
        [&](const auto& record) {
            return record.identity_token == identity_token;
        });
    return sidecar.records.size() != old_size;
}

}  // namespace autocattery::protection
