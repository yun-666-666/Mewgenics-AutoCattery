#include "settings_form.hpp"

#include <array>

#include "settings_form_schema.hpp"

namespace autocattery::settings_app {
namespace {

constexpr int kWindowMargin = 18;
constexpr int kGroupTop = 72;
constexpr int kGroupWidth = 382;
constexpr int kColumnGap = 12;
constexpr int kRowHeight = 29;
constexpr int kTextLeft = 214;
constexpr int kTextWidth = 140;

HWND AddControl(
    HWND parent,
    const wchar_t* class_name,
    const wchar_t* text,
    DWORD style,
    DWORD extended_style,
    int x,
    int y,
    int width,
    int height,
    int id,
    HFONT font) {
    auto* control = CreateWindowExW(
        extended_style,
        class_name,
        text,
        WS_CHILD | WS_VISIBLE | style,
        x,
        y,
        width,
        height,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr),
        nullptr);
    if (control != nullptr) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
    return control;
}

}  // namespace

void CreateSettingsForm(HWND parent, HFONT font) {
    AddControl(
        parent, L"STATIC", L"AutoCattery 游戏外规则编辑器",
        SS_LEFT, 0, kWindowMargin, 14, 660, 30, 0, font);
    AddControl(
        parent, L"STATIC",
        L"保存到 user_config.json；游戏未启动时下次启动生效，游戏运行且工作流空闲时自动热加载。",
        SS_LEFT, 0, kWindowMargin, 43, 1120, 24, 0, font);

    const auto groups = FieldGroups();
    for (std::size_t column = 0; column < groups.size(); ++column) {
        const auto x = kWindowMargin +
            static_cast<int>(column) * (kGroupWidth + kColumnGap);
        const auto height = 42 +
            static_cast<int>(groups[column].fields.size()) * kRowHeight;
        AddControl(
            parent, L"BUTTON", groups[column].title, BS_GROUPBOX, 0,
            x, kGroupTop, kGroupWidth, height, 0, font);
        int y = kGroupTop + 28;
        for (const auto& field : groups[column].fields) {
            const auto id = static_cast<int>(field.field);
            if (field.kind == FieldKind::Check) {
                AddControl(
                    parent, L"BUTTON", field.label, BS_AUTOCHECKBOX | WS_TABSTOP,
                    0, x + 14, y, kGroupWidth - 28, 24, id, font);
            } else {
                AddControl(
                    parent, L"STATIC", field.label, SS_LEFT, 0,
                    x + 14, y + 3, kTextLeft - 24, 22, 0, font);
                AddControl(
                    parent, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP,
                    WS_EX_CLIENTEDGE, x + kTextLeft, y, kTextWidth, 24, id, font);
            }
            y += kRowHeight;
        }
    }

    AddControl(
        parent, L"STATIC", L"", SS_LEFT, 0,
        kWindowMargin, 655, 830, 28, kStatusLabelId, font);
    AddControl(
        parent, L"BUTTON", L"重新读取", BS_PUSHBUTTON | WS_TABSTOP, 0,
        906, 650, 120, 34, kReloadButtonId, font);
    AddControl(
        parent, L"BUTTON", L"保存配置", BS_DEFPUSHBUTTON | WS_TABSTOP, 0,
        1038, 650, 140, 34, kSaveButtonId, font);
}

}  // namespace autocattery::settings_app
