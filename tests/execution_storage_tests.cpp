#include "auto_cattery/execution/backup_service.hpp"
#include "auto_cattery/execution/journal_store.hpp"
#include "auto_cattery/execution/recovery_package.hpp"

#include <chrono>
#include <fstream>

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

std::filesystem::path TempRoot() {
    return std::filesystem::temp_directory_path() /
           ("auto-cattery-stage10-" +
            std::to_string(
                std::chrono::steady_clock::now()
                    .time_since_epoch().count()));
}

void WriteBytes(
    const std::filesystem::path& path,
    std::string_view value) {
    std::ofstream output(path, std::ios::binary);
    output.write(value.data(), static_cast<std::streamsize>(value.size()));
}

}  // namespace

void RunExecutionStorageTests() {
    const auto root = TempRoot();
    std::filesystem::create_directories(root);
    const auto source = root / "fixture.sav";
    WriteBytes(source, "synthetic sqlite fixture bytes");

    execution::BackupService backups(root / "backups");
    const auto live = backups.CreateVerifiedBackup({
        "op-live", source, false
    });
    AC_CHECK(!live);
    AC_CHECK(!std::filesystem::exists(
        root / "backups" / "op-live" / "original.savbak"));

    WriteBytes(
        std::filesystem::path(source.wstring() + L"-wal"),
        "synthetic wal");
    const auto wal = backups.CreateVerifiedBackup({
        "op-wal", source, true
    });
    AC_CHECK(!wal);
    std::filesystem::remove(
        std::filesystem::path(source.wstring() + L"-wal"));

    const auto backup = backups.CreateVerifiedBackup({
        "op-safe", source, true
    });
    AC_CHECK(static_cast<bool>(backup));
    AC_CHECK(std::filesystem::exists(backup.value.backup_file));
    AC_CHECK(backup.value.byte_size == 30);
    AC_CHECK(
        execution::HashFile(source).value ==
        execution::HashFile(backup.value.backup_file).value);

    const auto overwrite = backups.CreateVerifiedBackup({
        "op-safe", source, true
    });
    AC_CHECK(!overwrite);
    AC_CHECK(
        execution::HashFile(backup.value.backup_file).value ==
        backup.value.content_hash);

    const auto traversal = backups.CreateVerifiedBackup({
        "../escape", source, true
    });
    AC_CHECK(!traversal);

    execution::OperationJournalEntry journal{
        .operation_id = "op-safe",
        .precondition = {
            .scene_generation = 9,
            .game_build_identity = "build",
            .save_identity = "save",
            .snapshot_content_digest = "snapshot",
            .classification_digest = "classification",
            .plan_digest = {"plan"},
            .protection_digest = 17
        },
        .status = execution::JournalStatus::Prepared,
        .records = {{
            .operation_index = 0,
            .old_room = "r1",
            .new_room = "r2"
        }},
        .backup_identity = backup.value.backup_identity
    };
    execution::JournalStore journals(root / "journals");
    AC_CHECK(static_cast<bool>(journals.Write(journal)));
    journal.status = execution::JournalStatus::Committed;
    AC_CHECK(static_cast<bool>(journals.Write(journal)));
    const auto journal_path =
        root / "journals" / "op-safe" / "operation-journal.json";
    std::ifstream journal_input(journal_path, std::ios::binary);
    const std::string journal_text{
        std::istreambuf_iterator<char>(journal_input),
        std::istreambuf_iterator<char>()};
    AC_CHECK(journal_text.find("Committed") != std::string::npos);
    AC_CHECK(journal_text.find("display_name") == std::string::npos);
    AC_CHECK(journal_text.find("\"cat_id\"") == std::string::npos);
    journal_input.close();

    execution::RecoveryPackageWriter recovery(root / "recovery");
    const auto package = recovery.Write("op-safe", backup.value);
    AC_CHECK(static_cast<bool>(package));
    AC_CHECK(std::filesystem::exists(package.value));

    std::filesystem::remove_all(root);
}

}  // namespace autocattery::tests
