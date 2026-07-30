#include "auto_cattery/snapshot/detail/win_sqlite_api.hpp"

#include "test_support.hpp"

namespace autocattery::tests {

void RunWinSqliteApiTests() {
    const auto& api = snapshot::detail::WinSqliteApi::Instance();
    AC_CHECK(api.Available());
    AC_CHECK(api.VersionNumber() > 0);
    AC_CHECK(!snapshot::detail::WinSqliteApi::LoadFrom(
        "relative-sqlite3.dll").Available());
}

}  // namespace autocattery::tests
