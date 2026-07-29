#pragma once

#include <windows.h>

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

    [[nodiscard]] bool Available() const noexcept;
    static const WinSqliteApi& Instance() noexcept;
};

}  // namespace autocattery::snapshot::detail
