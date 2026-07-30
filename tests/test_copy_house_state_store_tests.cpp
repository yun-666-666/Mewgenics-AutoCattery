#include "auto_cattery/save_safety/test_copy_house_state_store.hpp"

#include <chrono>
#include <fstream>

#include "auto_cattery/snapshot/detail/win_sqlite_api.hpp"
#include "auto_cattery/snapshot/house_state_writer.hpp"
#include "test_support.hpp"

namespace autocattery::tests {
namespace {

std::string Hex(std::span<const std::uint8_t> bytes) {
    constexpr char digits[] = "0123456789ABCDEF";
    std::string result;
    result.reserve(bytes.size() * 2);
    for (const auto byte : bytes) {
        result.push_back(digits[byte >> 4]);
        result.push_back(digits[byte & 0x0F]);
    }
    return result;
}

void CreateSave(
    const std::filesystem::path& path,
    std::span<const std::uint8_t> house_state,
    bool duplicate_row = false) {
    const auto& api = snapshot::detail::WinSqliteApi::Instance();
    snapshot::detail::sqlite3* database = nullptr;
    AC_CHECK(api.open_v2(path.string().c_str(), &database,
        0x00000002 | 0x00000004, nullptr) == 0);
    const auto duplicate = duplicate_row
        ? "INSERT INTO files VALUES('house_state',X'00');"
        : "";
    const auto sql =
        "CREATE TABLE properties (key TEXT, data INTEGER);"
        "CREATE TABLE cats (key INTEGER, data BLOB);"
        "CREATE TABLE files (key TEXT, data BLOB);"
        "INSERT INTO files VALUES('house_state',X'" + Hex(house_state) +
        "');" + duplicate;
    AC_CHECK(api.exec(database, sql.c_str(), nullptr, nullptr, nullptr) == 0);
    AC_CHECK(api.close_v2(database) == 0);
}

}  // namespace

void RunTestCopyHouseStateStoreTests() {
    if (!snapshot::detail::WinSqliteApi::Instance().Available()) {
        return;
    }
    const auto root = std::filesystem::temp_directory_path() /
        ("auto-cattery-house-store-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    const auto first = snapshot::SerializeHouseState(
        std::vector<snapshot::HouseStateEntry>{{1, "Floor1_Large", 1, 2, 3}});
    const auto second = snapshot::SerializeHouseState(
        std::vector<snapshot::HouseStateEntry>{{1, "Attic", 4, 5, 6}});
    AC_CHECK(first && second);
    const auto save = root / "copy.sav";
    CreateSave(save, first.value);
    save_safety::TestCopyHouseStateStore store;
    AC_CHECK(store.Read(save).value == first.value);
    AC_CHECK(static_cast<bool>(store.Write(save, second.value)));
    AC_CHECK(store.Read(save).value == second.value);
    std::ofstream(save.wstring() + L"-wal") << "sidecar";
    AC_CHECK(!store.Write(save, first.value));
    std::filesystem::remove(save.wstring() + L"-wal");

    const auto duplicate = root / "duplicate.sav";
    CreateSave(duplicate, first.value, true);
    AC_CHECK(!store.Write(duplicate, second.value));
    std::filesystem::remove_all(root);
}

}  // namespace autocattery::tests
