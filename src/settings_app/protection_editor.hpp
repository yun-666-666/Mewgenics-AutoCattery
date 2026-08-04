#pragma once

#include <filesystem>

#include <windows.h>

namespace autocattery::settings_app {

int RunProtectionEditor(
    HINSTANCE instance,
    int show_command,
    const std::filesystem::path& config_root,
    HWND owner);

}  // namespace autocattery::settings_app
