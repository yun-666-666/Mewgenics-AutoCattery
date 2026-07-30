#include "auto_cattery/save_safety/save_stability.hpp"

#include <windows.h>

#include <chrono>
#include <fstream>
#include <thread>

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

void Write(const std::filesystem::path& path, std::string_view text) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << text;
}

}  // namespace

void RunSaveStabilityTests() {
    const auto root = std::filesystem::temp_directory_path() /
        ("auto-cattery-stability-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    const auto save = root / "fixture.sav";
    Write(save, "stable");

    auto stable = save_safety::StableSaveGuard::Acquire(
        save, std::chrono::milliseconds(5));
    AC_CHECK(static_cast<bool>(stable));
    AC_CHECK(stable.value.ByteSize() == 6);
    stable = {};

    Write(std::filesystem::path(save.wstring() + L"-wal"), "wal");
    AC_CHECK(!save_safety::StableSaveGuard::Acquire(
        save, std::chrono::milliseconds(0)));
    std::filesystem::remove(
        std::filesystem::path(save.wstring() + L"-wal"));
    Write(std::filesystem::path(save.wstring() + L"-shm"), "shm");
    AC_CHECK(!save_safety::StableSaveGuard::Acquire(
        save, std::chrono::milliseconds(0)));
    std::filesystem::remove(
        std::filesystem::path(save.wstring() + L"-shm"));

    HANDLE writer = CreateFileW(
        save.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    AC_CHECK(writer != INVALID_HANDLE_VALUE);
    const auto occupied = save_safety::StableSaveGuard::Acquire(
        save, std::chrono::milliseconds(0));
    AC_CHECK(!occupied);
    AC_CHECK(occupied.message.find("occupied") != std::string::npos);
    CloseHandle(writer);

    std::thread modifier([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        Write(save, "changing-save");
    });
    const auto changing = save_safety::StableSaveGuard::Acquire(
        save, std::chrono::milliseconds(80));
    modifier.join();
    AC_CHECK(!changing);
    AC_CHECK(changing.message.find("changing") != std::string::npos);

    std::filesystem::remove_all(root);
}

}  // namespace autocattery::tests
