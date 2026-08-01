#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

#include "auto_cattery/protection/editor_model.hpp"

#include <windows.h>

namespace autocattery::settings_app::protection_ui {

inline constexpr int kSaveCombo = 3001;
inline constexpr int kReload = 3002;
inline constexpr int kCatList = 3003;
inline constexpr int kLevelCombo = 3004;
inline constexpr int kRoomCombo = 3005;
inline constexpr int kApply = 3006;
inline constexpr int kRemove = 3007;
inline constexpr int kStatus = 3008;

struct State {
    State(
        std::filesystem::path sidecar,
        std::filesystem::path game_root)
        : model(std::move(sidecar), std::move(game_root)) {}

    protection::ProtectionEditorModel model;
    HFONT font{};
};

std::wstring Wide(std::string_view text);
void CreateControls(HWND parent, HFONT font);
void PopulateAll(HWND window, State& state);
void PopulateCats(HWND window, State& state);
void PopulateCatEditor(HWND window, const State& state);
void ShowError(HWND window, std::string_view message);

}  // namespace autocattery::settings_app::protection_ui
