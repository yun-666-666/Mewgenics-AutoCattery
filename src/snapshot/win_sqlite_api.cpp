#include "auto_cattery/snapshot/detail/win_sqlite_api.hpp"

namespace autocattery::snapshot::detail {
namespace {

template<class Function>
Function Resolve(HMODULE module, const char* name) {
    return reinterpret_cast<Function>(GetProcAddress(module, name));
}

WinSqliteApi Load() {
    WinSqliteApi api;
    api.module = LoadLibraryExW(
        L"winsqlite3.dll",
        nullptr,
        LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (api.module == nullptr) {
        return api;
    }

    api.open_v2 = Resolve<WinSqliteApi::OpenV2>(
        api.module,
        "sqlite3_open_v2");
    api.close_v2 = Resolve<WinSqliteApi::CloseV2>(
        api.module,
        "sqlite3_close_v2");
    api.prepare_v2 = Resolve<WinSqliteApi::PrepareV2>(
        api.module,
        "sqlite3_prepare_v2");
    api.step = Resolve<WinSqliteApi::Step>(api.module, "sqlite3_step");
    api.finalize = Resolve<WinSqliteApi::Finalize>(
        api.module,
        "sqlite3_finalize");
    api.column_int64 = Resolve<WinSqliteApi::ColumnInt64>(
        api.module,
        "sqlite3_column_int64");
    api.column_blob = Resolve<WinSqliteApi::ColumnBlob>(
        api.module,
        "sqlite3_column_blob");
    api.column_bytes = Resolve<WinSqliteApi::ColumnBytes>(
        api.module,
        "sqlite3_column_bytes");
    api.column_text = Resolve<WinSqliteApi::ColumnText>(
        api.module,
        "sqlite3_column_text");
    api.error_message = Resolve<WinSqliteApi::ErrorMessage>(
        api.module,
        "sqlite3_errmsg");
    api.exec = Resolve<WinSqliteApi::Exec>(api.module, "sqlite3_exec");
    api.busy_timeout = Resolve<WinSqliteApi::BusyTimeout>(
        api.module,
        "sqlite3_busy_timeout");
    return api;
}

}  // namespace

bool WinSqliteApi::Available() const noexcept {
    return module != nullptr &&
           open_v2 != nullptr &&
           close_v2 != nullptr &&
           prepare_v2 != nullptr &&
           step != nullptr &&
           finalize != nullptr &&
           column_int64 != nullptr &&
           column_blob != nullptr &&
           column_bytes != nullptr &&
           column_text != nullptr &&
           error_message != nullptr &&
           exec != nullptr &&
           busy_timeout != nullptr;
}

const WinSqliteApi& WinSqliteApi::Instance() noexcept {
    static const WinSqliteApi api = Load();
    return api;
}

}  // namespace autocattery::snapshot::detail
