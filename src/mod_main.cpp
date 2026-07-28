#include <windows.h>

#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>

#include "auto_cattery/api_types.hpp"
#include "auto_cattery/config.hpp"
#include "auto_cattery/logger.hpp"
#include "auto_cattery/module_registry.hpp"
#include "auto_cattery/ui/mew_ui_bridge.hpp"
#include "auto_cattery/version.hpp"
#include "mewjector.h"

namespace autocattery {

int InitializeExport();

namespace {

std::mutex g_lifecycle_mutex;
std::atomic<bool> g_initialized{false};
HMODULE g_module{};
HANDLE g_initialize_thread{};
DWORD g_initialize_thread_id{};
MewjectorAPI g_mewjector{};
ModuleRegistry g_modules;
ModMode g_mode{ModMode::ReadOnly};

std::filesystem::path ModulePath() {
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(
        g_module,
        buffer.data(),
        static_cast<DWORD>(buffer.size()));
    buffer.resize(length);
    return std::filesystem::path(buffer);
}

std::filesystem::path ResolveModRoot(const std::filesystem::path& dll_path) {
    const auto parent = dll_path.parent_path();
    if (_wcsicmp(parent.filename().c_str(), L"mods") == 0) {
        return parent / L"AutoCattery";
    }
    return parent;
}

std::filesystem::path ResolveGameRoot(const std::filesystem::path& dll_path) {
    auto current = dll_path.parent_path();
    for (int depth = 0; depth < 3 && !current.empty(); ++depth) {
        if (std::filesystem::exists(current / L"Mewgenics.exe")) {
            return current;
        }
        current = current.parent_path();
    }
    return dll_path.parent_path();
}

DWORD WINAPI InitializeThread(void*) {
    InitializeExport();
    return 0;
}

}  // namespace

int InitializeExport() {
    std::scoped_lock lock(g_lifecycle_mutex);
    if (g_initialized.load()) {
        return 1;
    }

    const auto dll_path = ModulePath();
    InitContext context{
        ResolveGameRoot(dll_path),
        ResolveModRoot(dll_path),
        "unknown"
    };

    auto& logger = Logger::Instance();
    logger.Initialize(context.mod_root / L"logs");

    if (MJ_Require("AutoCattery") && MJ_Resolve(&g_mewjector)) {
        logger.AttachMewjector(&g_mewjector);
        logger.Write(
            LogLevel::Info,
            "Bootstrap",
            "AC1000",
            "Mewjector API resolved.");
    } else {
        logger.Write(
            LogLevel::Warn,
            "Bootstrap",
            "AC1101",
            "Mewjector API v3 is unavailable; continuing in read-only mode.");
    }

    logger.Write(
        LogLevel::Info,
        "Bootstrap",
        "AC1001",
        std::string(kModName) + " initializing.");

    const auto config = LoadConfig(
        context.mod_root / L"config" / L"default_config.json",
        context.mod_root / L"config" / L"user_config.json");
    if (!config) {
        g_mode = ModMode::ReadOnly;
        logger.Write(
            LogLevel::Error,
            "Config",
            "AC1301",
            "Configuration rejected; safe defaults and read-only mode are active.");
    } else if (
        config.value.force_read_only ||
        config.value.safe_mode ||
        (config.value.safety.abort_on_unknown_game_build &&
         context.game_build_id == "unknown")) {
        g_mode = ModMode::ReadOnly;
    } else {
        g_mode = ModMode::Full;
    }

    ui::MewUiBridge* ui_bridge_view{};
    if (g_modules.Size() == 0) {
        auto ui_bridge = std::make_unique<ui::MewUiBridge>();
        ui_bridge_view = ui_bridge.get();
        if (!g_modules.Register(std::move(ui_bridge))) {
            logger.Write(
                LogLevel::Error,
                "Bootstrap",
                "AC1901",
                "Failed to register core modules.");
            return 0;
        }
    }
    const auto modules = g_modules.InitializeAll(context);
    if (!modules) {
        logger.Write(
            LogLevel::Error,
            "Bootstrap",
            "AC1902",
            modules.message);
        return 0;
    }
    if (ui_bridge_view == nullptr || !ui_bridge_view->Available()) {
        g_mode = ModMode::CompatibilityDegraded;
    }

    g_initialized.store(true);
    logger.Write(
        LogLevel::Info,
        "Bootstrap",
        "AC1002",
        "Initialization completed without game-state access.");
    return 1;
}

void ShutdownExport() noexcept {
    if (g_initialize_thread != nullptr &&
        GetCurrentThreadId() != g_initialize_thread_id) {
        WaitForSingleObject(g_initialize_thread, INFINITE);
        CloseHandle(g_initialize_thread);
        g_initialize_thread = nullptr;
        g_initialize_thread_id = 0;
    }

    std::scoped_lock lock(g_lifecycle_mutex);
    if (!g_initialized.exchange(false)) {
        return;
    }
    g_modules.ShutdownAll();
    Logger::Instance().Write(
        LogLevel::Info,
        "Bootstrap",
        "AC1003",
        "AutoCattery shutdown completed.");
}

}  // namespace autocattery

extern "C" __declspec(dllexport) int AutoCattery_Initialize() {
    return autocattery::InitializeExport();
}

extern "C" __declspec(dllexport) void AutoCattery_Shutdown() {
    autocattery::ShutdownExport();
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        autocattery::g_module = module;
        DisableThreadLibraryCalls(module);
        autocattery::g_initialize_thread = CreateThread(
            nullptr,
            0,
            autocattery::InitializeThread,
            nullptr,
            0,
            &autocattery::g_initialize_thread_id);
    }
    return TRUE;
}
