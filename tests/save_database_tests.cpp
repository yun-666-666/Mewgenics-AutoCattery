#include "auto_cattery/snapshot/detail/save_database.hpp"
#include "auto_cattery/snapshot/detail/win_sqlite_api.hpp"
#include "auto_cattery/snapshot/detail/unlocked_breeding_data.hpp"

#include "test_support.hpp"

#include <windows.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace autocattery::tests {
namespace {

std::filesystem::path CreateFixture() {
    wchar_t temporary_directory[MAX_PATH]{};
    wchar_t temporary_file[MAX_PATH]{};
    GetTempPathW(MAX_PATH, temporary_directory);
    GetTempFileNameW(temporary_directory, L"act", 0, temporary_file);
    DeleteFileW(temporary_file);

    const auto path = std::filesystem::path(temporary_file);
    const std::string utf8_path = path.string();
    const auto& api = snapshot::detail::WinSqliteApi::Instance();
    snapshot::detail::sqlite3* database = nullptr;
    constexpr int kOpenReadWriteCreate = 0x00000002 | 0x00000004;
    const int opened = api.open_v2(
        utf8_path.c_str(), &database, kOpenReadWriteCreate, nullptr);
    AC_CHECK(opened == 0);
    if (opened != 0 || database == nullptr) {
        return path;
    }

    constexpr const char* kSchemaAndData =
        "CREATE TABLE properties(key TEXT PRIMARY KEY,data INTEGER);"
        "CREATE TABLE cats(key INTEGER PRIMARY KEY,data BLOB);"
        "CREATE TABLE files(key TEXT PRIMARY KEY,data BLOB);"
        "INSERT INTO properties VALUES('current_day',17);"
        "INSERT INTO cats VALUES(42,X'010203');"
        "INSERT INTO files VALUES('house_state',X'04050607');";
    AC_CHECK(api.exec(
        database, kSchemaAndData, nullptr, nullptr, nullptr) == 0);
    // Three empty versioned pedigree tables, with no Tink progress record.
    std::vector<std::uint8_t> pedigree;
    for (int table = 0; table < 3; ++table) {
        pedigree.insert(pedigree.end(), {0xf5, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff});
        pedigree.resize(pedigree.size() + 16, 0);
        pedigree.resize(pedigree.size() + 17, 0x80);
        pedigree.resize(pedigree.size() + 8, 0);
    }
    snapshot::detail::sqlite3_stmt* statement = nullptr;
    AC_CHECK(api.prepare_v2(database, "INSERT INTO files VALUES('pedigree',?)",
        -1, &statement, nullptr) == 0);
    AC_CHECK(api.bind_blob(statement, 1, pedigree.data(),
        static_cast<int>(pedigree.size()), nullptr) == 0);
    AC_CHECK(api.step(statement) == 101);
    AC_CHECK(api.finalize(statement) == 0);
    AC_CHECK(api.close_v2(database) == 0);
    return path;
}

}  // namespace

void RunSaveDatabaseTests() {
    const auto fixture = CreateFixture();
    std::string error;
    {
        auto database =
            snapshot::detail::SaveDatabase::OpenReadOnly(fixture, error);
        AC_CHECK(database != nullptr);
        if (database != nullptr) {
            std::optional<std::int32_t> day;
            AC_CHECK(database->ReadCurrentDay(day, error));
            AC_CHECK(day == 17);

            std::vector<snapshot::detail::CatStorageRecord> cats;
            AC_CHECK(database->ReadCats(cats, error));
            AC_CHECK(cats.size() == 1);
            AC_CHECK(cats.front().id == 42);
            AC_CHECK(cats.front().blob.size() == 3);

            std::optional<std::vector<std::byte>> house_state;
            AC_CHECK(database->ReadHouseState(house_state, error));
            AC_CHECK(house_state.has_value());
            AC_CHECK(house_state->size() == 4);

            const auto breeding = snapshot::detail::LoadUnlockedBreedingData(*database);
            AC_CHECK(static_cast<bool>(breeding));
            AC_CHECK(!breeding.value.unlocks.pedigree);
            AC_CHECK(!breeding.value.unlocks.sexuality);
            AC_CHECK(breeding.value.pedigree.has_value());
        }
    }
    std::filesystem::remove(fixture);

    const auto missing = fixture.wstring() + L".missing";
    error.clear();
    auto absent = snapshot::detail::SaveDatabase::OpenReadOnly(
        std::filesystem::path(missing), error);
    AC_CHECK(absent == nullptr);
    AC_CHECK(!std::filesystem::exists(missing));
}

}  // namespace autocattery::tests
