#include "auto_cattery/save_safety/test_copy_house_state_store.hpp"

#include <windows.h>

#include <climits>
#include <cstring>
#include <optional>

#include "auto_cattery/snapshot/detail/save_database.hpp"
#include "auto_cattery/snapshot/detail/win_sqlite_api.hpp"

namespace autocattery::save_safety {
namespace {

constexpr int kSqliteOk = 0;
constexpr int kSqliteRow = 100;
constexpr int kSqliteDone = 101;
constexpr int kOpenReadWrite = 0x00000002;
constexpr int kOpenNoMutex = 0x00008000;

std::string Utf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
    return result;
}

Result<void> Failure(std::string message) {
    return {ErrorCode::WriteConflict, std::move(message)};
}

bool NoSidecars(const std::filesystem::path& save) {
    std::error_code error;
    auto journal = save;
    journal += L"-journal";
    auto wal = save;
    wal += L"-wal";
    auto shm = save;
    shm += L"-shm";
    return !std::filesystem::exists(journal, error) && !error &&
        !std::filesystem::exists(wal, error) && !error &&
        !std::filesystem::exists(shm, error) && !error;
}

}  // namespace

TestCopyHouseStateStore::TestCopyHouseStateStore()
    : write_api_(&snapshot::detail::WinSqliteApi::Instance()) {}

TestCopyHouseStateStore::TestCopyHouseStateStore(
    const snapshot::detail::WinSqliteApi& write_api)
    : write_api_(&write_api) {}

Result<std::vector<std::uint8_t>> TestCopyHouseStateStore::Read(
    const std::filesystem::path& save) const {
    std::string error;
    auto database = snapshot::detail::SaveDatabase::OpenReadOnly(save, error);
    std::optional<std::vector<std::byte>> blob;
    if (!database || !database->ReadHouseState(blob, error) || !blob) {
        return {{}, ErrorCode::RoomDataUnavailable,
            "test-copy house_state read failed: " + error};
    }
    std::vector<std::uint8_t> bytes(blob->size());
    if (!blob->empty()) {
        std::memcpy(bytes.data(), blob->data(), blob->size());
    }
    return {std::move(bytes)};
}

Result<void> TestCopyHouseStateStore::Write(
    const std::filesystem::path& save,
    std::span<const std::uint8_t> house_state) const {
    using snapshot::detail::WinSqliteApi;
    using snapshot::detail::sqlite3;
    using snapshot::detail::sqlite3_stmt;
    const auto& api = *write_api_;
    if (!api.Available() || house_state.empty() ||
        house_state.size() > static_cast<std::size_t>(INT_MAX) ||
        !NoSidecars(save)) {
        return Failure("test-copy SQLite write preconditions failed");
    }
    const auto utf8_path = Utf8(save.wstring());
    sqlite3* database = nullptr;
    if (utf8_path.empty() || api.open_v2(
            utf8_path.c_str(), &database,
            kOpenReadWrite | kOpenNoMutex, nullptr) != kSqliteOk) {
        if (database != nullptr) {
            api.close_v2(database);
        }
        return Failure("test-copy SQLite open failed");
    }

    bool transaction_started = false;
    sqlite3_stmt* update = nullptr;
    const auto close_with_failure = [&](std::string message) {
        if (update != nullptr) {
            api.finalize(update);
            update = nullptr;
        }
        if (transaction_started) {
            api.exec(database, "ROLLBACK", nullptr, nullptr, nullptr);
        }
        api.close_v2(database);
        return Failure(std::move(message));
    };
    const auto sqlite_failure = [&](std::string context) {
        const auto* detail = api.error_message(database);
        if (detail != nullptr && *detail != '\0') {
            context += ": ";
            context += detail;
        }
        return close_with_failure(std::move(context));
    };
    if (api.busy_timeout(database, 250) != kSqliteOk) {
        return sqlite_failure("test-copy SQLite busy timeout failed");
    }
    if (api.exec(database, "PRAGMA journal_mode=DELETE", nullptr, nullptr,
            nullptr) != kSqliteOk) {
        return sqlite_failure("test-copy SQLite journal mode failed");
    }
    if (api.exec(database, "PRAGMA synchronous=FULL", nullptr, nullptr,
            nullptr) != kSqliteOk) {
        return sqlite_failure("test-copy SQLite synchronous mode failed");
    }
    if (api.exec(database, "BEGIN IMMEDIATE TRANSACTION", nullptr, nullptr,
            nullptr) != kSqliteOk) {
        return sqlite_failure("test-copy SQLite transaction start failed");
    }
    transaction_started = true;
    if (api.prepare_v2(database,
            "UPDATE files SET data=?1 WHERE key='house_state'",
            -1, &update, nullptr) != kSqliteOk) {
        return close_with_failure("house_state update preparation failed");
    }
    auto transient = reinterpret_cast<WinSqliteApi::Destructor>(
        static_cast<std::intptr_t>(-1));
    if (api.bind_blob(update, 1, house_state.data(),
            static_cast<int>(house_state.size()), transient) != kSqliteOk ||
        api.step(update) != kSqliteDone || api.changes(database) != 1) {
        return close_with_failure("house_state update did not affect exactly one row");
    }
    api.finalize(update);
    update = nullptr;
    if (api.exec(database, "COMMIT", nullptr, nullptr, nullptr) != kSqliteOk) {
        return close_with_failure("test-copy SQLite commit failed");
    }
    transaction_started = false;

    sqlite3_stmt* integrity = nullptr;
    if (api.prepare_v2(database, "PRAGMA integrity_check", -1,
            &integrity, nullptr) != kSqliteOk ||
        api.step(integrity) != kSqliteRow) {
        if (integrity != nullptr) {
            api.finalize(integrity);
        }
        api.close_v2(database);
        return Failure("test-copy SQLite integrity check could not run");
    }
    const auto* text = api.column_text(integrity, 0);
    const bool integrity_ok = text != nullptr &&
        std::strcmp(reinterpret_cast<const char*>(text), "ok") == 0;
    api.finalize(integrity);
    const bool closed = api.close_v2(database) == kSqliteOk;
    if (!integrity_ok || !closed || !NoSidecars(save)) {
        return Failure("test-copy SQLite integrity or sidecar check failed");
    }
    return {};
}

}  // namespace autocattery::save_safety
