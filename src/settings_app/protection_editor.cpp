#include "protection_editor.hpp"

#include <array>

#include "protection_editor_internal.hpp"

namespace autocattery::settings_app {
namespace {

constexpr wchar_t kClassName[] = L"AutoCattery.ProtectionEditor.Window";

std::filesystem::path FindGameRoot(
    const std::filesystem::path& config_root) {
    const std::array candidates{
        config_root,
        config_root.parent_path(),
        config_root.parent_path().parent_path()
    };
    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate / L"resources.gpak")) {
            return candidate;
        }
    }
    return config_root.parent_path();
}

protection::ProtectionLevel SelectedLevel(HWND window) {
    using enum protection::ProtectionLevel;
    const auto index = SendDlgItemMessageW(
        window, protection_ui::kLevelCombo, CB_GETCURSEL, 0, 0);
    switch (index) {
        case 0: return NoCull;
        case 1: return NoMove;
        case 3: return FullyUnmanaged;
        default: return NoCullOrMove;
    }
}

std::optional<snapshot::RoomId> SelectedRoom(
    HWND window,
    const protection_ui::State& state) {
    const auto index = SendDlgItemMessageW(
        window, protection_ui::kRoomCombo, CB_GETCURSEL, 0, 0);
    if (index <= 0 ||
        static_cast<std::size_t>(index) > state.model.rooms().size()) {
        return std::nullopt;
    }
    return state.model.rooms()[static_cast<std::size_t>(index - 1)];
}

std::optional<std::size_t> SelectedCat(HWND window) {
    const auto index = SendDlgItemMessageW(
        window, protection_ui::kCatList, LB_GETCURSEL, 0, 0);
    return index == LB_ERR
        ? std::nullopt
        : std::optional<std::size_t>(static_cast<std::size_t>(index));
}

void Reload(HWND window, protection_ui::State& state) {
    const auto loaded = state.model.Reload();
    if (!loaded) {
        protection_ui::ShowError(window, loaded.message);
        return;
    }
    protection_ui::PopulateAll(window, state);
    SetDlgItemTextW(
        window, protection_ui::kStatus,
        L"选择一只猫并应用保护；没有选择就不会写入任何规则。");
}

void ApplyRule(HWND window, protection_ui::State& state) {
    const auto cat = SelectedCat(window);
    if (!cat) {
        protection_ui::ShowError(window, "please select a cat");
        return;
    }
    const auto result = state.model.Apply(
        *cat, SelectedLevel(window), SelectedRoom(window, state));
    if (!result) {
        protection_ui::ShowError(window, result.message);
        return;
    }
    protection_ui::PopulateCats(window, state);
    SendDlgItemMessageW(window, protection_ui::kCatList,
                        LB_SETCURSEL, *cat, 0);
    protection_ui::PopulateCatEditor(window, state);
    SetDlgItemTextW(window, protection_ui::kStatus,
                    L"保护规则已原子保存；下次预览会自动读取。");
}

void RemoveRule(HWND window, protection_ui::State& state) {
    const auto cat = SelectedCat(window);
    if (!cat) {
        protection_ui::ShowError(window, "please select a cat");
        return;
    }
    const auto result = state.model.Remove(*cat);
    if (!result) {
        protection_ui::ShowError(window, result.message);
        return;
    }
    protection_ui::PopulateCats(window, state);
    SendDlgItemMessageW(window, protection_ui::kCatList,
                        LB_SETCURSEL, *cat, 0);
    protection_ui::PopulateCatEditor(window, state);
    SetDlgItemTextW(window, protection_ui::kStatus,
                    L"该猫的 MOD 保护已移除。");
}

LRESULT CALLBACK Procedure(
    HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* state = reinterpret_cast<protection_ui::State*>(
        GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        state = static_cast<protection_ui::State*>(create->lpCreateParams);
        SetWindowLongPtrW(
            window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }
    if (message == WM_CREATE) {
        state->font = CreateFontW(
            -18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
        protection_ui::CreateControls(window, state->font);
        Reload(window, *state);
        return 0;
    }
    if (message == WM_COMMAND && state != nullptr) {
        const auto id = LOWORD(wparam);
        const auto event = HIWORD(wparam);
        if (id == protection_ui::kReload && event == BN_CLICKED) {
            Reload(window, *state);
        } else if (id == protection_ui::kSaveCombo &&
                   event == CBN_SELCHANGE) {
            const auto index = SendDlgItemMessageW(
                window, protection_ui::kSaveCombo, CB_GETCURSEL, 0, 0);
            if (index != CB_ERR && state->model.SelectSave(index)) {
                protection_ui::PopulateCats(window, *state);
            }
        } else if (id == protection_ui::kCatList && event == LBN_SELCHANGE) {
            protection_ui::PopulateCatEditor(window, *state);
        } else if (id == protection_ui::kApply && event == BN_CLICKED) {
            ApplyRule(window, *state);
        } else if (id == protection_ui::kRemove && event == BN_CLICKED) {
            RemoveRule(window, *state);
        }
        return 0;
    }
    if (message == WM_DESTROY) {
        if (state != nullptr && state->font != nullptr) {
            DeleteObject(state->font);
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

}  // namespace

int RunProtectionEditor(
    HINSTANCE instance,
    int show_command,
    const std::filesystem::path& config_root) {
    WNDCLASSEXW type{};
    type.cbSize = sizeof(type);
    type.hInstance = instance;
    type.lpfnWndProc = Procedure;
    type.lpszClassName = kClassName;
    type.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    type.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    if (RegisterClassExW(&type) == 0) {
        return 2;
    }
    protection_ui::State state(
        config_root / L"config" / L"protection.json",
        FindGameRoot(config_root));
    auto* window = CreateWindowExW(
        0, kClassName, L"AutoCattery 通用猫保护管理",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 720, 710,
        nullptr, nullptr, instance, &state);
    if (window == nullptr) {
        return 2;
    }
    ShowWindow(window, show_command);
    UpdateWindow(window);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(window, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    return static_cast<int>(message.wParam);
}

}  // namespace autocattery::settings_app
