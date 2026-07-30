#include "auto_cattery/snapshot/detail/win_sqlite_api.hpp"

namespace autocattery::snapshot::detail {
namespace {

template<class Function>
Function Resolve(HMODULE module, const char* name) {
    return reinterpret_cast<Function>(GetProcAddress(module, name));
}

WinSqliteApi Load(HMODULE module) {
    WinSqliteApi api;
    api.module = module;
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
    api.bind_blob = Resolve<WinSqliteApi::BindBlob>(
        api.module,
        "sqlite3_bind_blob");
    api.changes = Resolve<WinSqliteApi::Changes>(
        api.module,
        "sqlite3_changes");
    api.library_version_number = Resolve<WinSqliteApi::LibraryVersionNumber>(
        api.module,
        "sqlite3_libversion_number");
    api.initialize = Resolve<WinSqliteApi::Initialize>(
        api.module,
        "sqlite3_initialize");
    if (api.initialize == nullptr || api.initialize() != 0) {
        FreeLibrary(api.module);
        api.module = nullptr;
    }
    return api;
}

WinSqliteApi LoadSystem() {
    return Load(LoadLibraryExW(
        L"winsqlite3.dll",
        nullptr,
        LOAD_LIBRARY_SEARCH_SYSTEM32));
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
           busy_timeout != nullptr &&
           bind_blob != nullptr &&
           changes != nullptr &&
           library_version_number != nullptr &&
           initialize != nullptr;
}

int WinSqliteApi::VersionNumber() const noexcept {
    return library_version_number == nullptr ? 0 : library_version_number();
}

WinSqliteApi WinSqliteApi::LoadFrom(
    const std::filesystem::path& library) noexcept {
    if (!library.is_absolute()) {
        return {};
    }
    return Load(LoadLibraryExW(
        library.c_str(),
        nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32));
}

const WinSqliteApi& WinSqliteApi::Instance() noexcept {
    static const WinSqliteApi api = LoadSystem();
    return api;
}

}  // namespace autocattery::snapshot::detail
