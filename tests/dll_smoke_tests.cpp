#include <windows.h>

#include <filesystem>
#include <iostream>

namespace {

using InitializeFunction = int (*)();
using ShutdownFunction = void (*)();

bool LoadOnce(const std::filesystem::path& dll_path) {
    const HMODULE module = LoadLibraryW(dll_path.c_str());
    if (module == nullptr) {
        std::cerr << "LoadLibraryW failed: " << GetLastError() << '\n';
        return false;
    }

    const auto initialize = reinterpret_cast<InitializeFunction>(
        GetProcAddress(module, "AutoCattery_Initialize"));
    const auto shutdown = reinterpret_cast<ShutdownFunction>(
        GetProcAddress(module, "AutoCattery_Shutdown"));
    if (initialize == nullptr || shutdown == nullptr) {
        std::cerr << "Required exports were not found.\n";
        FreeLibrary(module);
        return false;
    }

    const bool initialized = initialize() == 1;
    shutdown();
    const bool unloaded = FreeLibrary(module) != FALSE;
    return initialized && unloaded;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) {
        std::cerr << "Usage: dll_smoke_tests <AutoCattery.dll>\n";
        return 2;
    }

    const std::filesystem::path dll_path(argv[1]);
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (!LoadOnce(dll_path)) {
            std::cerr << "DLL smoke attempt failed: " << attempt + 1 << '\n';
            return 1;
        }
    }
    return 0;
}
