#include "auto_cattery/execution/journal_store.hpp"

#include <fstream>

#include <nlohmann/json.hpp>

#include "file_safety.hpp"

namespace autocattery::execution {
namespace {

const char* StatusName(JournalStatus status) {
    switch (status) {
    case JournalStatus::Prepared: return "Prepared";
    case JournalStatus::Committed: return "Committed";
    case JournalStatus::RolledBack: return "RolledBack";
    case JournalStatus::ManualRecoveryRequired:
        return "ManualRecoveryRequired";
    }
    return "Unknown";
}

}  // namespace

JournalStore::JournalStore(std::filesystem::path journal_root)
    : journal_root_(std::move(journal_root)) {}

Result<void> JournalStore::Write(
    const OperationJournalEntry& entry) {
    if (!detail::IsSafeToken(entry.operation_id)) {
        return {
            ErrorCode::WriteConflict,
            "journal operation identity is unsafe"
        };
    }
    std::filesystem::path canonical_root;
    std::filesystem::path operation_root;
    if (!detail::PrepareContainedDirectory(
            journal_root_, entry.operation_id,
            canonical_root, operation_root)) {
        return {
            ErrorCode::WriteConflict,
            "journal destination is not safely contained"
        };
    }

    nlohmann::json document{
        {"schema_version", 1},
        {"operation_id", entry.operation_id},
        {"status", StatusName(entry.status)},
        {"backup_identity", entry.backup_identity},
        {"failure_reason",
         static_cast<int>(entry.failure_reason)},
        {"precondition", {
            {"scene_generation",
             entry.precondition.scene_generation},
            {"game_day", entry.precondition.game_day},
            {"game_build_identity",
             entry.precondition.game_build_identity},
            {"save_identity", entry.precondition.save_identity},
            {"snapshot_content_digest",
             entry.precondition.snapshot_content_digest},
            {"classification_digest",
             entry.precondition.classification_digest},
            {"plan_digest",
             entry.precondition.plan_digest.value},
            {"protection_digest",
             entry.precondition.protection_digest}
        }},
        {"records", nlohmann::json::array()}
    };
    for (const auto& record : entry.records) {
        document["records"].push_back({
            {"operation_index", record.operation_index},
            {"old_room", record.old_room},
            {"new_room", record.new_room},
            {"was_culled", record.was_culled}
        });
    }

    const auto temporary = operation_root / L"operation-journal.json.tmp";
    const auto destination = operation_root / L"operation-journal.json";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            return {
                ErrorCode::WriteConflict,
                "journal temporary file could not be opened"
            };
        }
        output << document.dump(2);
        output.flush();
        if (!output) {
            return {
                ErrorCode::WriteConflict,
                "journal temporary write failed"
            };
        }
    }
    if (!detail::AtomicPublish(temporary, destination, true)) {
        return {
            ErrorCode::WriteConflict,
            "atomic journal publication failed"
        };
    }
    return {};
}

}  // namespace autocattery::execution
