#include "auto_cattery/snapshot/detail/save_locator.hpp"

#include <algorithm>
#include <cstdlib>
#include <cwctype>

namespace autocattery::snapshot::detail {
namespace {

std::filesystem::path DefaultSaveRoot() {
    wchar_t* app_data = nullptr;
    std::size_t length = 0;
    if (_wdupenv_s(&app_data, &length, L"APPDATA") != 0 ||
        app_data == nullptr) {
        return {};
    }
    const std::filesystem::path result =
        std::filesystem::path(app_data) / L"Glaiel Games" / L"Mewgenics";
    std::free(app_data);
    return result;
}

std::wstring Lower(std::wstring value) {
    std::transform(
        value.begin(), value.end(), value.begin(),
        [](wchar_t value) { return std::towlower(value); });
    return value;
}

bool IsBackupPath(const std::filesystem::path& path) {
    for (const auto& component : path) {
        const std::wstring name = Lower(component.wstring());
        if (name == L"backup" || name == L"backups") {
            return true;
        }
    }
    return false;
}

bool IsSaveFile(const std::filesystem::path& path) {
    return Lower(path.extension().wstring()) == L".sav" &&
           !IsBackupPath(path);
}

}  // namespace

std::optional<std::filesystem::path> FindMostRecentSave(
    const std::filesystem::path& configured_root,
    std::string& error) {
    const std::filesystem::path root =
        configured_root.empty() ? DefaultSaveRoot() : configured_root;
    if (root.empty()) {
        error = "Mewgenics save root is unavailable";
        return std::nullopt;
    }

    std::error_code status_error;
    if (std::filesystem::is_regular_file(root, status_error)) {
        if (IsSaveFile(root)) {
            return root;
        }
        error = "configured save file is not a .sav file";
        return std::nullopt;
    }
    if (!std::filesystem::is_directory(root, status_error)) {
        error = "Mewgenics save root does not exist";
        return std::nullopt;
    }

    std::optional<std::filesystem::path> newest;
    std::filesystem::file_time_type newest_time{};
    std::error_code iteration_error;
    const auto options =
        std::filesystem::directory_options::skip_permission_denied;
    for (std::filesystem::recursive_directory_iterator iterator(
             root, options, iteration_error),
         end;
         iterator != end;
         iterator.increment(iteration_error)) {
        if (iteration_error) {
            iteration_error.clear();
            continue;
        }
        std::error_code file_error;
        if (!iterator->is_regular_file(file_error) ||
            !IsSaveFile(iterator->path())) {
            continue;
        }
        const auto modified =
            std::filesystem::last_write_time(iterator->path(), file_error);
        if (file_error) {
            continue;
        }
        if (!newest.has_value() || modified > newest_time ||
            (modified == newest_time &&
             iterator->path().wstring() > newest->wstring())) {
            newest = iterator->path();
            newest_time = modified;
        }
    }
    if (!newest.has_value()) {
        error = "no Mewgenics .sav file found";
    }
    return newest;
}

}  // namespace autocattery::snapshot::detail
