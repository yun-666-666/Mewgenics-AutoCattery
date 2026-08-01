#include "protection_editor_internal.hpp"

#include <algorithm>
#include <array>
#include <iterator>
#include <string_view>

namespace autocattery::settings_app::protection_ui {
namespace {

HWND Add(
    HWND parent, const wchar_t* type, const wchar_t* text,
    DWORD style, int x, int y, int width, int height, int id, HFONT font) {
    auto* control = CreateWindowExW(
        type == std::wstring_view(L"EDIT") ? WS_EX_CLIENTEDGE : 0,
        type, text, WS_CHILD | WS_VISIBLE | style,
        x, y, width, height, parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr), nullptr);
    if (control != nullptr) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
    return control;
}

const wchar_t* LevelName(protection::ProtectionLevel level) {
    using enum protection::ProtectionLevel;
    switch (level) {
        case NoCull: return L"禁止淘汰，允许移动";
        case NoMove: return L"禁止移动";
        case NoCullOrMove: return L"禁止淘汰和移动";
        case FullyUnmanaged: return L"完全不自动管理";
        case None: return L"无";
    }
    return L"无";
}

int LevelIndex(protection::ProtectionLevel level) {
    using enum protection::ProtectionLevel;
    switch (level) {
        case NoCull: return 0;
        case NoMove: return 1;
        case NoCullOrMove: return 2;
        case FullyUnmanaged: return 3;
        case None: return 2;
    }
    return 2;
}

}  // namespace

std::wstring Wide(std::string_view text) {
    if (text.empty()) {
        return {};
    }
    const auto size = MultiByteToWideChar(
        CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(
        CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
        result.data(), size);
    return result;
}

void CreateControls(HWND parent, HFONT font) {
    Add(parent, L"STATIC", L"通用猫保护管理", SS_LEFT,
        18, 14, 400, 30, 0, font);
    Add(parent, L"STATIC", L"存档只用于列出猫；规则绑定猫指纹，不绑定存档文件。",
        SS_LEFT, 18, 44, 650, 24, 0, font);
    Add(parent, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP,
        18, 76, 510, 300, kSaveCombo, font);
    Add(parent, L"BUTTON", L"重新读取存档", BS_PUSHBUTTON | WS_TABSTOP,
        540, 75, 140, 28, kReload, font);
    Add(parent, L"LISTBOX", L"", LBS_NOTIFY | WS_VSCROLL | WS_BORDER |
        WS_TABSTOP, 18, 116, 662, 390, kCatList, font);
    Add(parent, L"STATIC", L"保护级别", SS_LEFT,
        18, 525, 90, 24, 0, font);
    auto* levels = Add(parent, L"COMBOBOX", L"",
        CBS_DROPDOWNLIST | WS_TABSTOP, 110, 521, 250, 200,
        kLevelCombo, font);
    const std::array level_names{
        L"禁止淘汰，允许移动", L"禁止移动",
        L"禁止淘汰和移动", L"完全不自动管理"};
    for (const auto* name : level_names) {
        SendMessageW(levels, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name));
    }
    SendMessageW(levels, CB_SETCURSEL, 2, 0);
    Add(parent, L"STATIC", L"固定房间", SS_LEFT,
        378, 525, 80, 24, 0, font);
    Add(parent, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP,
        460, 521, 220, 200, kRoomCombo, font);
    Add(parent, L"BUTTON", L"应用保护", BS_DEFPUSHBUTTON | WS_TABSTOP,
        398, 565, 135, 32, kApply, font);
    Add(parent, L"BUTTON", L"移除保护", BS_PUSHBUTTON | WS_TABSTOP,
        545, 565, 135, 32, kRemove, font);
    Add(parent, L"STATIC", L"", SS_LEFT,
        18, 610, 662, 42, kStatus, font);
}

void PopulateAll(HWND window, State& state) {
    auto* combo = GetDlgItem(window, kSaveCombo);
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    for (const auto& save : state.model.saves()) {
        const auto label = Wide(save.label);
        SendMessageW(combo, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(label.c_str()));
    }
    SendMessageW(combo, CB_SETCURSEL,
                 static_cast<WPARAM>(state.model.selected_save()), 0);
    PopulateCats(window, state);
}

void PopulateCats(HWND window, State& state) {
    auto* list = GetDlgItem(window, kCatList);
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    for (const auto& cat : state.model.cats()) {
        std::wstring label = cat.level ? L"[已保护] " : L"[未保护] ";
        label += Wide(cat.display_name) + L"  #" +
            std::to_wstring(cat.cat_id) + L"  |  " +
            Wide(cat.room_id.value_or("房外"));
        if (cat.level) {
            label += L"  |  " + std::wstring(LevelName(*cat.level));
        }
        SendMessageW(list, LB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(label.c_str()));
    }
    auto* rooms = GetDlgItem(window, kRoomCombo);
    SendMessageW(rooms, CB_RESETCONTENT, 0, 0);
    SendMessageW(rooms, CB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(L"不固定房间"));
    for (const auto& room : state.model.rooms()) {
        const auto label = Wide(room);
        SendMessageW(rooms, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(label.c_str()));
    }
    SendMessageW(rooms, CB_SETCURSEL, 0, 0);
}

void PopulateCatEditor(HWND window, const State& state) {
    const auto selected = SendDlgItemMessageW(
        window, kCatList, LB_GETCURSEL, 0, 0);
    if (selected == LB_ERR ||
        static_cast<std::size_t>(selected) >= state.model.cats().size()) {
        return;
    }
    const auto& cat = state.model.cats()[static_cast<std::size_t>(selected)];
    SendDlgItemMessageW(window, kLevelCombo, CB_SETCURSEL,
        cat.level ? LevelIndex(*cat.level) : 2, 0);
    int room_index{};
    if (cat.fixed_room) {
        const auto found = std::ranges::find(
            state.model.rooms(), *cat.fixed_room);
        if (found != state.model.rooms().end()) {
            room_index = static_cast<int>(
                std::distance(state.model.rooms().begin(), found)) + 1;
        }
    }
    SendDlgItemMessageW(window, kRoomCombo, CB_SETCURSEL, room_index, 0);
}

void ShowError(HWND window, std::string_view message) {
    const auto text = Wide(message);
    SetDlgItemTextW(window, kStatus, text.c_str());
    MessageBoxW(window, text.c_str(), L"保护规则错误", MB_OK | MB_ICONERROR);
}

}  // namespace autocattery::settings_app::protection_ui
