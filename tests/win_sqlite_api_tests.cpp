#include "auto_cattery/snapshot/detail/win_sqlite_api.hpp"

#include "test_support.hpp"

namespace autocattery::tests {

void RunWinSqliteApiTests() {
    const auto& api = snapshot::detail::WinSqliteApi::Instance();
    AC_CHECK(api.Available());
    AC_CHECK(api.VersionNumber() > 0);
    AC_CHECK(!snapshot::detail::WinSqliteApi::LoadFrom(
        "relative-sqlite3.dll").Available());

    const auto folder = std::filesystem::temp_directory_path() /
        (L"ac-online-backup-" + std::to_wstring(GetCurrentProcessId()));
    std::filesystem::create_directories(folder);
    const auto source = folder / "source.sav";
    const auto destination = folder / "recovery.sav";
    snapshot::detail::sqlite3* writer{};
    const auto path = source.u8string();
    AC_CHECK(api.open_v2(reinterpret_cast<const char*>(path.c_str()), &writer, 6, nullptr) == 0);
    AC_CHECK(api.exec(writer, "PRAGMA journal_mode=WAL; CREATE TABLE cats(id); INSERT INTO cats VALUES(42); BEGIN IMMEDIATE; INSERT INTO cats VALUES(99);", nullptr, nullptr, nullptr) == 0);
    // The game-like writable connection remains open with an uncommitted WAL write.
    AC_CHECK(static_cast<bool>(api.BackupReadOnly(source, destination)));
    snapshot::detail::sqlite3* reader{};
    const auto target = destination.u8string();
    AC_CHECK(api.open_v2(reinterpret_cast<const char*>(target.c_str()), &reader, 1, nullptr) == 0);
    snapshot::detail::sqlite3_stmt* statement{};
    AC_CHECK(api.prepare_v2(reader, "SELECT count(*), max(id) FROM cats", -1, &statement, nullptr) == 0);
    AC_CHECK(api.step(statement) == 100);
    AC_CHECK(api.column_int64(statement, 0) == 1);
    AC_CHECK(api.column_int64(statement, 1) == 42);
    api.finalize(statement);
    api.close_v2(reader);
    AC_CHECK(!api.BackupReadOnly(source, destination));
    api.exec(writer, "ROLLBACK", nullptr, nullptr, nullptr);
    api.close_v2(writer);
    std::filesystem::remove(destination);
    std::filesystem::remove(source);
    for (const auto name : {"source.sav-wal", "source.sav-shm", "recovery.sav-wal", "recovery.sav-shm"})
        std::filesystem::remove(folder / name);
    std::filesystem::remove(folder);
}

}  // namespace autocattery::tests
