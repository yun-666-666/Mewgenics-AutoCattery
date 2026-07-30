#include "auto_cattery/execution/backup_service.hpp"
#include "auto_cattery/save_safety/atomic_file_replace.hpp"
#include "auto_cattery/save_safety/backup_catalog.hpp"
#include "auto_cattery/save_safety/game_process_probe.hpp"
#include "auto_cattery/save_safety/restore_service.hpp"

#include <windows.h>

#include <iostream>
#include <string>
#include <unordered_map>

namespace {

std::string Utf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }
    std::string text(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), text.data(), size, nullptr, nullptr);
    return text;
}

bool Has(const std::unordered_map<std::wstring, std::wstring>& arguments,
         std::wstring_view name) {
    return arguments.contains(std::wstring(name));
}

std::wstring Get(
    const std::unordered_map<std::wstring, std::wstring>& arguments,
    std::wstring_view name) {
    const auto found = arguments.find(std::wstring(name));
    return found == arguments.end() ? std::wstring{} : found->second;
}

int Fail(const std::string& message) {
    std::cerr << message << '\n';
    return 1;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    std::unordered_map<std::wstring, std::wstring> arguments;
    for (int index = 1; index < argc; ++index) {
        const std::wstring key(argv[index]);
        if (key == L"--list" || key == L"--verify-only" || key == L"--restore") {
            arguments.emplace(key, L"");
            continue;
        }
        if (index + 1 >= argc || key.rfind(L"--", 0) != 0) {
            return Fail("invalid restore command arguments");
        }
        arguments.emplace(key, argv[++index]);
    }
    const int modes = static_cast<int>(Has(arguments, L"--list")) +
        static_cast<int>(Has(arguments, L"--verify-only")) +
        static_cast<int>(Has(arguments, L"--restore"));
    const auto root = Get(arguments, L"--backup-root");
    if (modes != 1 || root.empty()) {
        return Fail("use exactly one mode and provide --backup-root");
    }

    autocattery::execution::BackupService backups(root);
    autocattery::save_safety::BackupCatalog catalog(root);
    autocattery::save_safety::WindowsGameProcessProbe processes;
    autocattery::save_safety::AtomicFileReplacer replacer;
    autocattery::save_safety::RestoreService restore(
        catalog, backups, processes, replacer);
    if (Has(arguments, L"--list")) {
        for (const auto& item : catalog.List()) {
            std::cout << item.operation_id << ' '
                      << (item.manifest_valid ? "valid" : "invalid") << '\n';
        }
        return 0;
    }

    const auto operation_id = Utf8(Get(arguments, L"--operation-id"));
    if (operation_id.empty()) {
        return Fail("--operation-id is required");
    }
    if (Has(arguments, L"--verify-only")) {
        const auto verified = restore.Verify(operation_id);
        return verified ? 0 : Fail(verified.message);
    }

    const auto restore_id = Utf8(Get(arguments, L"--restore-operation-id"));
    const auto target = Get(arguments, L"--target-save");
    const auto executable = Get(arguments, L"--game-executable");
    if (restore_id.empty() || target.empty() || executable.empty()) {
        return Fail("restore requires target, game executable, and restore operation ID");
    }
    const auto outcome = restore.Restore({
        .backup_operation_id = operation_id,
        .restore_operation_id = restore_id,
        .target_save = target,
        .game_executable = executable
    });
    return outcome ? 0 : Fail(outcome.message);
}
