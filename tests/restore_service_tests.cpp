#include "auto_cattery/save_safety/restore_service.hpp"
#include "auto_cattery/snapshot/detail/save_database.hpp"
#include "auto_cattery/snapshot/detail/win_sqlite_api.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <optional>

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

void Write(const std::filesystem::path& path, std::string_view text) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << text;
}

void CreateSqliteSave(const std::filesystem::path& path) {
    const auto& api = snapshot::detail::WinSqliteApi::Instance();
    snapshot::detail::sqlite3* database = nullptr;
    const int opened = api.open_v2(
        path.string().c_str(), &database, 0x00000002 | 0x00000004, nullptr);
    AC_CHECK(opened == 0);
    AC_CHECK(api.exec(database,
        "CREATE TABLE properties (key TEXT, data INTEGER);"
        "INSERT INTO properties VALUES ('current_day', 32);"
        "CREATE TABLE cats (key INTEGER, data BLOB);"
        "CREATE TABLE files (key TEXT, data BLOB);",
        nullptr, nullptr, nullptr) == 0);
    AC_CHECK(api.close_v2(database) == 0);
}

std::string Read(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), {}};
}

struct FakeProcesses final : save_safety::IGameProcessProbe {
    bool running{};
    int calls{};
    int start_on_call{-1};
    Result<bool> IsRunning(const std::filesystem::path&) override {
        ++calls;
        return {running || calls == start_on_call};
    }
};

struct DeniedReplace final : save_safety::IAtomicFileReplacer {
    Result<void> ReplaceTemporary(
        const std::filesystem::path&,
        const std::filesystem::path&) override {
        return {ErrorCode::WriteConflict, "access denied by injected file policy"};
    }
};

struct CorruptOnceReplace final : save_safety::IAtomicFileReplacer {
    int calls{};
    save_safety::AtomicFileReplacer actual;
    Result<void> ReplaceTemporary(
        const std::filesystem::path& temporary,
        const std::filesystem::path& destination) override {
        const auto result = actual.ReplaceTemporary(temporary, destination);
        if (result && ++calls == 1) {
            Write(destination, "incorrect-readback");
        }
        return result;
    }
};

save_safety::RestoreRequest Request(
    const std::filesystem::path& target,
    std::string restore_id) {
    return {
        .backup_operation_id = "original",
        .restore_operation_id = std::move(restore_id),
        .target_save = target,
        .game_executable = "synthetic-game.exe",
        .stable_window = std::chrono::milliseconds(1)
    };
}

}  // namespace

void RunRestoreServiceTests() {
    if (!snapshot::detail::WinSqliteApi::Instance().Available()) {
        return;
    }
    const auto root = std::filesystem::temp_directory_path() /
        ("auto-cattery-restore-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    const auto target = root / "target.sav";
    const auto source = root / "source.sav";
    CreateSqliteSave(source);
    Write(target, "broken-before-restore");
    execution::BackupService backups(root / "backups");
    const auto original = backups.CreateVerifiedBackup({
        "original", source, true, "unknown", {}, std::chrono::milliseconds(1)
    });
    AC_CHECK(static_cast<bool>(original));
    save_safety::BackupCatalog catalog(root / "backups");
    FakeProcesses processes;
    save_safety::AtomicFileReplacer replacer;
    save_safety::RestoreService restore(catalog, backups, processes, replacer);

    processes.running = true;
    const auto running = restore.Restore(Request(target, "restore-running"));
    AC_CHECK(!running);
    AC_CHECK(Read(target) == "broken-before-restore");
    AC_CHECK(!std::filesystem::exists(
        root / "backups" / "restore-running" / "original.savbak"));

    processes.running = false;
    processes.calls = 0;
    processes.start_on_call = 2;
    const auto starts_during_prepare = restore.Restore(
        Request(target, "restore-started"));
    AC_CHECK(!starts_during_prepare);
    AC_CHECK(Read(target) == "broken-before-restore");
    AC_CHECK(std::filesystem::exists(
        root / "backups" / "restore-started" / "original.savbak"));

    processes.calls = 0;
    processes.start_on_call = -1;
    const auto first = restore.Restore(Request(target, "restore-one"));
    AC_CHECK(static_cast<bool>(first));
    std::string read_error;
    auto restored_database = snapshot::detail::SaveDatabase::OpenReadOnly(
        target, read_error);
    AC_CHECK(restored_database != nullptr);
    std::optional<std::int32_t> restored_day;
    AC_CHECK(restored_database->ReadCurrentDay(restored_day, read_error));
    AC_CHECK(restored_day == 32);
    restored_database.reset();
    AC_CHECK(static_cast<bool>(backups.VerifyBackup(
        first.value.pre_restore_backup)));
    AC_CHECK(Read(first.value.pre_restore_backup.backup_file) ==
        "broken-before-restore");

    Write(target, "broken-again");
    const auto repeated = restore.Restore(Request(target, "restore-two"));
    AC_CHECK(static_cast<bool>(repeated));
    restored_database = snapshot::detail::SaveDatabase::OpenReadOnly(
        target, read_error);
    AC_CHECK(restored_database != nullptr);
    restored_database.reset();
    const auto duplicate = restore.Restore(Request(target, "restore-two"));
    AC_CHECK(!duplicate);
    restored_database = snapshot::detail::SaveDatabase::OpenReadOnly(
        target, read_error);
    AC_CHECK(restored_database != nullptr);
    restored_database.reset();

    Write(target, "permission-test");
    DeniedReplace denied;
    save_safety::RestoreService denied_restore(
        catalog, backups, processes, denied);
    const auto denied_result = denied_restore.Restore(
        Request(target, "restore-denied"));
    AC_CHECK(!denied_result);
    AC_CHECK(Read(target) == "permission-test");

    Write(target, "rollback-test");
    CorruptOnceReplace corrupting;
    save_safety::RestoreService corrupting_restore(
        catalog, backups, processes, corrupting);
    const auto mismatch = corrupting_restore.Restore(
        Request(target, "restore-mismatch"));
    AC_CHECK(!mismatch);
    AC_CHECK(mismatch.message.find("pre-restore backup was restored") !=
        std::string::npos);
    AC_CHECK(Read(target) == "rollback-test");

    AC_CHECK(!catalog.Find("../escape"));
    Write(root / "backups" / "original" / "manifest.json", "invalid manifest");
    const auto listings = catalog.List();
    const auto original_listing = std::find_if(
        listings.begin(), listings.end(), [](const auto& item) {
            return item.operation_id == "original";
        });
    AC_CHECK(original_listing != listings.end());
    AC_CHECK(!original_listing->manifest_valid);
    std::filesystem::remove_all(root);
}

}  // namespace autocattery::tests
