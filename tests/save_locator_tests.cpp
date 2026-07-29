#include "auto_cattery/snapshot/detail/save_locator.hpp"

#include "test_support.hpp"

#include <windows.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace autocattery::tests {
namespace {

std::filesystem::path MakeTemporaryDirectory() {
    wchar_t temporary_root[MAX_PATH]{};
    wchar_t temporary_name[MAX_PATH]{};
    GetTempPathW(MAX_PATH, temporary_root);
    GetTempFileNameW(temporary_root, L"acl", 0, temporary_name);
    DeleteFileW(temporary_name);
    std::filesystem::create_directory(temporary_name);
    return temporary_name;
}

void Touch(const std::filesystem::path& path) {
    std::ofstream file(path, std::ios::binary);
    file.put('\0');
}

}  // namespace

void RunSaveLocatorTests() {
    const auto root = MakeTemporaryDirectory();
    const auto saves = root / "profile" / "saves";
    const auto backups = root / "backups";
    std::filesystem::create_directories(saves);
    std::filesystem::create_directories(backups);

    const auto older = saves / "steamcampaign01.sav";
    const auto newer = saves / "steamcampaign02.sav";
    Touch(older);
    Touch(newer);
    Touch(backups / "newest.sav");
    const auto now = std::filesystem::file_time_type::clock::now();
    std::filesystem::last_write_time(older, now - std::chrono::seconds(2));
    std::filesystem::last_write_time(newer, now - std::chrono::seconds(1));
    std::filesystem::last_write_time(
        backups / "newest.sav", now + std::chrono::seconds(1));

    std::string error;
    const auto selected =
        snapshot::detail::FindMostRecentSave(root, error);
    AC_CHECK(selected == newer);

    error.clear();
    const auto explicit_file =
        snapshot::detail::FindMostRecentSave(older, error);
    AC_CHECK(explicit_file == older);

    std::filesystem::remove_all(root);
}

}  // namespace autocattery::tests
