#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "auto_cattery/settings_file_editor.hpp"
#include "protection_editor.hpp"
#include "settings_form.hpp"
#include "settings_form_schema.hpp"

#include <shellapi.h>
#include <windows.h>

namespace autocattery::settings_app {
namespace {

constexpr wchar_t kWindowClass[] = L"AutoCattery.Settings.Window";
constexpr wchar_t kWindowTitle[] = L"AutoCattery 游戏外规则编辑器";

struct AppState {
    AppState(SettingsFilePaths paths, std::filesystem::path config_root)
        : editor(std::move(paths)), root(std::move(config_root)) {}

    SettingsFileEditor editor;
    std::filesystem::path root;
    Config config;
    HFONT font{};
};

std::wstring Utf8ToWide(std::string_view text) {
    if (text.empty()) {
        return {};
    }
    const auto size = MultiByteToWideChar(
        CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) {
        return L"配置错误";
    }
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(
        CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
        result.data(), size);
    return result;
}

void SetStatus(HWND window, const wchar_t* text) {
    SetDlgItemTextW(window, kStatusLabelId, text);
}

void LaunchProtectionEditor(HWND window, const AppState& state) {
    std::wstring executable(32768, L'\0');
    const auto length = GetModuleFileNameW(
        nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    executable.resize(length);
    const std::wstring arguments =
        L"--protection --config-root \"" + state.root.wstring() + L"\"";
    const auto launched = reinterpret_cast<INT_PTR>(ShellExecuteW(
        window, L"open", executable.c_str(), arguments.c_str(),
        state.root.c_str(), SW_SHOWNORMAL));
    SetStatus(
        window,
        launched > 32 ? L"已打开通用猫保护管理。"
                      : L"无法打开猫保护管理。" );
}

bool LoadIntoForm(HWND window, AppState& state, bool show_error) {
    const auto loaded = state.editor.Load();
    if (!loaded) {
        const auto message = Utf8ToWide(loaded.message);
        SetStatus(window, L"读取失败，原配置未修改。");
        if (show_error) {
            MessageBoxW(window, message.c_str(), L"配置读取失败", MB_OK | MB_ICONERROR);
        }
        return false;
    }
    state.config = loaded.value;
    PopulateSettingsForm(window, state.config);
    SetStatus(window, L"配置已读取。修改后点击“保存配置”。");
    return true;
}

void SaveForm(HWND window, AppState& state) {
    const auto candidate = ReadSettingsForm(window, state.config);
    if (!candidate) {
        const auto message = Utf8ToWide(candidate.message);
        SetStatus(window, L"输入无效，未保存。");
        MessageBoxW(window, message.c_str(), L"配置输入无效", MB_OK | MB_ICONWARNING);
        return;
    }
    if (!state.config.execution_safety.single_click_execute &&
        candidate.value.execution_safety.single_click_execute) {
        const auto answer = MessageBoxW(
            window,
            L"单击执行会减少一次确认，但不会绕过预览、保护和备份硬约束。确定启用吗？",
            L"确认危险设置",
            MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
        if (answer != IDYES) {
            CheckDlgButton(
                window,
                static_cast<int>(SettingField::SingleClickExecute),
                BST_UNCHECKED);
            SetStatus(window, L"已取消启用单击执行，未保存。");
            return;
        }
    }

    const auto saved = state.editor.Save(candidate.value);
    if (!saved) {
        const auto message = Utf8ToWide(saved.message);
        SetStatus(window, L"验证或保存失败，原配置未修改。");
        MessageBoxW(window, message.c_str(), L"保存失败", MB_OK | MB_ICONERROR);
        return;
    }
    state.config = saved.value;
    PopulateSettingsForm(window, state.config);
    SetStatus(window, L"已安全保存到 user_config.json。可以进入游戏。");
}

LRESULT CALLBACK WindowProcedure(
    HWND window,
    UINT message,
    WPARAM wparam,
    LPARAM lparam) {
    auto* state = reinterpret_cast<AppState*>(
        GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        state = static_cast<AppState*>(create->lpCreateParams);
        SetWindowLongPtrW(
            window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }
    switch (message) {
    case WM_CREATE:
        state->font = CreateFontW(
            -18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
        CreateSettingsForm(window, state->font);
        LoadIntoForm(window, *state, true);
        return 0;
    case WM_COMMAND:
        if (state != nullptr && HIWORD(wparam) == BN_CLICKED) {
            if (LOWORD(wparam) == kSaveButtonId) {
                SaveForm(window, *state);
                return 0;
            }
            if (LOWORD(wparam) == kReloadButtonId) {
                LoadIntoForm(window, *state, true);
                return 0;
            }
            if (LOWORD(wparam) == kProtectionButtonId) {
                LaunchProtectionEditor(window, *state);
                return 0;
            }
        }
        break;
    case WM_DESTROY:
        if (state != nullptr && state->font != nullptr) {
            DeleteObject(state->font);
            state->font = nullptr;
        }
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

std::filesystem::path ExecutableDirectory() {
    std::wstring buffer(32768, L'\0');
    const auto length = GetModuleFileNameW(
        nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    buffer.resize(length);
    return std::filesystem::path(buffer).parent_path();
}

std::filesystem::path ConfigRoot(int argument_count, wchar_t** arguments) {
    for (int index = 1; index + 1 < argument_count; ++index) {
        if (std::wstring_view(arguments[index]) == L"--config-root") {
            return std::filesystem::absolute(arguments[index + 1]);
        }
    }
    return ExecutableDirectory();
}

bool HasArgument(
    int argument_count,
    wchar_t** arguments,
    std::wstring_view expected) {
    for (int index = 1; index < argument_count; ++index) {
        if (std::wstring_view(arguments[index]) == expected) {
            return true;
        }
    }
    return false;
}

int ValidateCommand(int argument_count, wchar_t** arguments) {
    if (argument_count < 3 || std::wstring_view(arguments[1]) != L"--validate") {
        return -1;
    }
    const auto root = std::filesystem::absolute(arguments[2]);
    SettingsFileEditor editor({
        root / L"config" / L"default_config.json",
        root / L"config" / L"user_config.json"
    });
    return editor.Load() ? 0 : 2;
}

}  // namespace
}  // namespace autocattery::settings_app

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    using namespace autocattery::settings_app;
    int argument_count{};
    auto** arguments = CommandLineToArgvW(GetCommandLineW(), &argument_count);
    if (arguments == nullptr) {
        return 2;
    }
    const auto validation = ValidateCommand(argument_count, arguments);
    if (validation >= 0) {
        LocalFree(arguments);
        return validation;
    }
    const auto root = ConfigRoot(argument_count, arguments);
    const bool protection_editor = HasArgument(
        argument_count, arguments, L"--protection");
    LocalFree(arguments);

    if (protection_editor) {
        return RunProtectionEditor(instance, show_command, root);
    }

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.hInstance = instance;
    window_class.lpfnWndProc = &WindowProcedure;
    window_class.lpszClassName = kWindowClass;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    if (RegisterClassExW(&window_class) == 0) {
        return 2;
    }

    AppState state(
        {
            root / L"config" / L"default_config.json",
            root / L"config" / L"user_config.json"
        },
        root);
    auto* window = CreateWindowExW(
        0, kWindowClass, kWindowTitle,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 1216, 735,
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
