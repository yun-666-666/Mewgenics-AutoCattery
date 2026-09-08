#pragma once

#include <windows.h>

#include <filesystem>
#include "auto_cattery/error.hpp"

namespace autocattery::snapshot::detail {

struct sqlite3;
struct sqlite3_stmt;

struct WinSqliteApi {
    using OpenV2 = int(__cdecl*)(const char*, sqlite3**, int, const char*);
    using CloseV2 = int(__cdecl*)(sqlite3*);
    using PrepareV2 =
        int(__cdecl*)(sqlite3*, const char*, int, sqlite3_stmt**, const char**);
    using Step = int(__cdecl*)(sqlite3_stmt*);
    using Finalize = int(__cdecl*)(sqlite3_stmt*);
    using ColumnInt64 = long long(__cdecl*)(sqlite3_stmt*, int);
    using ColumnBlob = const void*(__cdecl*)(sqlite3_stmt*, int);
    using ColumnBytes = int(__cdecl*)(sqlite3_stmt*, int);
    using ColumnText = const unsigned char*(__cdecl*)(sqlite3_stmt*, int);
    using ErrorMessage = const char*(__cdecl*)(sqlite3*);
    using Exec =
        int(__cdecl*)(sqlite3*, const char*, void*, void*, char**);
    using BusyTimeout = int(__cdecl*)(sqlite3*, int);
    using Destructor = void(__cdecl*)(void*);
    using BindBlob = int(__cdecl*)(
        sqlite3_stmt*, int, const void*, int, Destructor);
    using Changes = int(__cdecl*)(sqlite3*);
    using LibraryVersionNumber = int(__cdecl*)();
    using Initialize = int(__cdecl*)();

    HMODULE module{};
    OpenV2 open_v2{};
    CloseV2 close_v2{};
    PrepareV2 prepare_v2{};
    Step step{};
    Finalize finalize{};
    ColumnInt64 column_int64{};
    ColumnBlob column_blob{};
    ColumnBytes column_bytes{};
    ColumnText column_text{};
    ErrorMessage error_message{};
    Exec exec{};
    BusyTimeout busy_timeout{};
    BindBlob bind_blob{};
    Changes changes{};
    LibraryVersionNumber library_version_number{};
    Initialize initialize{};

    [[nodiscard]] bool Available() const noexcept;
    [[nodiscard]] int VersionNumber() const noexcept;
    static WinSqliteApi LoadFrom(
        const std::filesystem::path& library) noexcept;
    static const WinSqliteApi& Instance() noexcept;
    Result<void> BackupReadOnly(const std::filesystem::path& source,
        const std::filesystem::path& destination) const;
};

}  // namespace autocattery::snapshot::detail
