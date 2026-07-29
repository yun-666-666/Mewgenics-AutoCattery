#include "auto_cattery/snapshot/detail/win_sqlite_api.hpp"

#include "test_support.hpp"

namespace autocattery::tests {

void RunWinSqliteApiTests() {
    const auto& api = snapshot::detail::WinSqliteApi::Instance();
    AC_CHECK(api.Available());
}

}  // namespace autocattery::tests
