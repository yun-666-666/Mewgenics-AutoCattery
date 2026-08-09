#include "auto_cattery/snapshot/detail/save_database.hpp"

#include "auto_cattery/snapshot/detail/win_sqlite_api.hpp"

#include <limits>
#include <string_view>

namespace autocattery::snapshot::detail {
namespace {

constexpr int kSqliteOk = 0;
constexpr int kSqliteRow = 100;
constexpr int kSqliteDone = 101;
constexpr int kOpenReadOnly = 0x00000001;
constexpr int kOpenNoMutex = 0x00008000;

std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(
        CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
        nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
        result.data(), size, nullptr, nullptr);
    return result;
}

std::string DatabaseError(sqlite3* database, std::string_view prefix) {
    const auto& api = WinSqliteApi::Instance();
    const char* detail =
        database != nullptr ? api.error_message(database) : nullptr;
    std::string result(prefix);
    if (detail != nullptr) {
        result.append(": ").append(detail);
    }
    return result;
}

class Statement final {
public:
    Statement(sqlite3* database, const char* sql, std::string& error)
        : database_(database) {
        const auto& api = WinSqliteApi::Instance();
        if (api.prepare_v2(database, sql, -1, &statement_, nullptr) !=
            kSqliteOk) {
            error = DatabaseError(database, "prepare failed");
        }
    }

    ~Statement() {
        if (statement_ != nullptr) {
            WinSqliteApi::Instance().finalize(statement_);
        }
    }

    [[nodiscard]] sqlite3_stmt* get() const noexcept {
        return statement_;
    }

private:
    sqlite3* database_{};
    sqlite3_stmt* statement_{};
};

bool CopyBlob(
    sqlite3_stmt* statement,
    int column,
    std::vector<std::byte>& destination,
    std::string& error) {
    const auto& api = WinSqliteApi::Instance();
    const int size = api.column_bytes(statement, column);
    const void* data = api.column_blob(statement, column);
    if (size < 0 || (size > 0 && data == nullptr)) {
        error = "invalid SQLite blob";
        return false;
    }
    if (size == 0) {
        destination.clear();
        return true;
    }
    const auto* begin = static_cast<const std::byte*>(data);
    destination.assign(begin, begin + size);
    return true;
}

}  // namespace

SaveDatabase::SaveDatabase(sqlite3* database) noexcept
    : database_(database) {}

SaveDatabase::~SaveDatabase() {
    if (database_ == nullptr) {
        return;
    }
    const auto& api = WinSqliteApi::Instance();
    api.exec(database_, "ROLLBACK", nullptr, nullptr, nullptr);
    api.close_v2(database_);
}

std::unique_ptr<SaveDatabase> SaveDatabase::OpenReadOnly(
    const std::filesystem::path& path,
    std::string& error) {
    const auto& api = WinSqliteApi::Instance();
    if (!api.Available()) {
        error = "system SQLite API unavailable";
        return nullptr;
    }
    const std::string utf8_path = WideToUtf8(path.wstring());
    if (utf8_path.empty()) {
        error = "save path is empty or cannot be encoded";
        return nullptr;
    }

    sqlite3* database = nullptr;
    const int open_result = api.open_v2(
        utf8_path.c_str(),
        &database,
        kOpenReadOnly | kOpenNoMutex,
        nullptr);
    if (open_result != kSqliteOk) {
        error = DatabaseError(database, "read-only open failed");
        if (database != nullptr) {
            api.close_v2(database);
        }
        return nullptr;
    }
    if (api.busy_timeout(database, 250) != kSqliteOk ||
        api.exec(database, "PRAGMA writable_schema=ON", nullptr, nullptr,
                 nullptr) != kSqliteOk ||
        api.exec(database, "BEGIN DEFERRED TRANSACTION", nullptr, nullptr,
                 nullptr) != kSqliteOk) {
        error = DatabaseError(database, "read transaction failed");
        api.close_v2(database);
        return nullptr;
    }
    return std::unique_ptr<SaveDatabase>(new SaveDatabase(database));
}

bool SaveDatabase::ReadCurrentDay(
    std::optional<std::int32_t>& day,
    std::string& error) const {
    Statement statement(
        database_,
        "SELECT data FROM properties WHERE key='current_day' LIMIT 1",
        error);
    if (statement.get() == nullptr) {
        return false;
    }
    const auto& api = WinSqliteApi::Instance();
    const int result = api.step(statement.get());
    if (result == kSqliteDone) {
        day.reset();
        return true;
    }
    if (result != kSqliteRow) {
        error = DatabaseError(database_, "current day query failed");
        return false;
    }
    const long long value = api.column_int64(statement.get(), 0);
    if (value < 0 || value > std::numeric_limits<std::int32_t>::max()) {
        error = "current day is outside the supported range";
        return false;
    }
    day = static_cast<std::int32_t>(value);
    return true;
}

bool SaveDatabase::ReadCats(
    std::vector<CatStorageRecord>& cats,
    std::string& error) const {
    Statement statement(
        database_, "SELECT key, data FROM cats ORDER BY key", error);
    if (statement.get() == nullptr) {
        return false;
    }
    const auto& api = WinSqliteApi::Instance();
    cats.clear();
    for (;;) {
        const int result = api.step(statement.get());
        if (result == kSqliteDone) {
            return true;
        }
        if (result != kSqliteRow) {
            error = DatabaseError(database_, "cat query failed");
            return false;
        }
        CatStorageRecord record;
        record.id = api.column_int64(statement.get(), 0);
        if (!CopyBlob(statement.get(), 1, record.blob, error)) {
            return false;
        }
        cats.push_back(std::move(record));
    }
}

bool SaveDatabase::ReadHouseState(
    std::optional<std::vector<std::byte>>& blob,
    std::string& error) const {
    Statement statement(
        database_,
        "SELECT data FROM files WHERE key='house_state' LIMIT 1",
        error);
    if (statement.get() == nullptr) {
        return false;
    }
    const auto& api = WinSqliteApi::Instance();
    const int result = api.step(statement.get());
    if (result == kSqliteDone) {
        blob.reset();
        return true;
    }
    if (result != kSqliteRow) {
        error = DatabaseError(database_, "house state query failed");
        return false;
    }
    std::vector<std::byte> value;
    if (!CopyBlob(statement.get(), 0, value, error)) {
        return false;
    }
    blob = std::move(value);
    return true;
}

bool SaveDatabase::ReadFurniture(
    std::vector<FurnitureStorageRecord>& furniture,
    std::string& error) const {
    Statement statement(
        database_, "SELECT key, data FROM furniture ORDER BY key", error);
    if (statement.get() == nullptr) {
        return false;
    }
    const auto& api = WinSqliteApi::Instance();
    furniture.clear();
    for (;;) {
        const int result = api.step(statement.get());
        if (result == kSqliteDone) {
            return true;
        }
        if (result != kSqliteRow) {
            error = DatabaseError(database_, "furniture query failed");
            return false;
        }
        FurnitureStorageRecord record;
        record.key = static_cast<std::int64_t>(
            api.column_int64(statement.get(), 0));
        if (!CopyBlob(statement.get(), 1, record.blob, error)) {
            return false;
        }
        furniture.push_back(std::move(record));
    }
}

}  // namespace autocattery::snapshot::detail
