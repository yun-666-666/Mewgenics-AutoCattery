#include "auto_cattery/save_safety/game_process_probe.hpp"

#include <windows.h>
#include <tlhelp32.h>

namespace autocattery::save_safety {
namespace {

bool SamePath(
    const std::filesystem::path& left,
    const std::filesystem::path& right) {
    return _wcsicmp(left.c_str(), right.c_str()) == 0;
}

}  // namespace

Result<bool> WindowsGameProcessProbe::IsRunning(
    const std::filesystem::path& game_executable) {
    std::error_code error;
    const auto target = std::filesystem::weakly_canonical(game_executable, error);
    if (error || !std::filesystem::is_regular_file(target, error) || error) {
        return {{}, ErrorCode::OperationCancelled,
            "a verified game executable path is required for restore"};
    }
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return {{}, ErrorCode::OperationCancelled,
            "game process state could not be inspected"};
    }
    PROCESSENTRY32W entry{.dwSize = sizeof(entry)};
    for (BOOL found = Process32FirstW(snapshot, &entry);
         found != FALSE;
         found = Process32NextW(snapshot, &entry)) {
        HANDLE process = OpenProcess(
            PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
        if (process == nullptr) {
            if (_wcsicmp(entry.szExeFile, target.filename().c_str()) == 0) {
                CloseHandle(snapshot);
                return {{}, ErrorCode::OperationCancelled,
                    "matching game process could not be inspected"};
            }
            continue;
        }
        std::wstring image(32768, L'\0');
        DWORD length = static_cast<DWORD>(image.size());
        const BOOL queried = QueryFullProcessImageNameW(
            process, 0, image.data(), &length);
        CloseHandle(process);
        if (!queried) {
            continue;
        }
        image.resize(length);
        const auto current = std::filesystem::weakly_canonical(image, error);
        if (!error && SamePath(current, target)) {
            CloseHandle(snapshot);
            return {true};
        }
        error.clear();
    }
    CloseHandle(snapshot);
    return {false};
}

}  // namespace autocattery::save_safety
