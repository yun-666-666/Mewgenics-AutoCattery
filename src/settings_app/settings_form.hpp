#pragma once

#include <string>
#include <string_view>

#include "auto_cattery/config.hpp"

#include <windows.h>

namespace autocattery::settings_app {

inline constexpr int kSaveButtonId = 1001;
inline constexpr int kReloadButtonId = 1002;
inline constexpr int kStatusLabelId = 1003;

void CreateSettingsForm(HWND parent, HFONT font);
void PopulateSettingsForm(HWND parent, const Config& config);
[[nodiscard]] Result<Config> ReadSettingsForm(
    HWND parent,
    const Config& base);

}  // namespace autocattery::settings_app
