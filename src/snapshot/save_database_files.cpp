#include "auto_cattery/snapshot/detail/save_database.hpp"

#include "auto_cattery/snapshot/detail/win_sqlite_api.hpp"

#include <string>

namespace autocattery::snapshot::detail {

bool SaveDatabase::ReadFileBlob(
    const char* key,
    std::optional<std::vector<std::byte>>& blob,
    std::string& error) const {
    if (key == nullptr || *key == '\0') {
        error = "file blob key is empty";
        return false;
    }
    const std::string sql =
        "SELECT data FROM files WHERE key='" + std::string(key) +
        "' LIMIT 1";
    sqlite3_stmt* statement{};
    const auto& api = WinSqliteApi::Instance();
    if (api.prepare_v2(
            database_, sql.c_str(), -1, &statement, nullptr) != 0) {
        error = "file blob query prepare failed";
        return false;
    }
    struct Finalizer {
        sqlite3_stmt* statement;
        ~Finalizer() {
            WinSqliteApi::Instance().finalize(statement);
        }
    } finalizer{statement};
    const int result = api.step(statement);
    if (result == 101) {
        blob.reset();
        return true;
    }
    if (result != 100) {
        error = "file blob query failed";
        return false;
    }
    const int size = api.column_bytes(statement, 0);
    const auto* data = static_cast<const std::byte*>(
        api.column_blob(statement, 0));
    if (size < 0 || (size > 0 && data == nullptr)) {
        error = "file blob is invalid";
        return false;
    }
    blob = std::vector<std::byte>(data, data + size);
    return true;
}

}  // namespace autocattery::snapshot::detail
