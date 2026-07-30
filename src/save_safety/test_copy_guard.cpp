#include "auto_cattery/save_safety/test_copy_guard.hpp"

#include <cstdlib>

#include "auto_cattery/save_safety/safe_path.hpp"

namespace autocattery::save_safety {
namespace {

std::filesystem::path PlayerSaveRoot() {
    wchar_t* app_data = nullptr;
    std::size_t length{};
    if (_wdupenv_s(&app_data, &length, L"APPDATA") != 0 ||
        app_data == nullptr) {
        return {};
    }
    const std::filesystem::path root =
        std::filesystem::path(app_data) / L"Glaiel Games" / L"Mewgenics";
    std::free(app_data);
    return root;
}

bool Overlaps(
    const std::filesystem::path& left,
    const std::filesystem::path& right) {
    return IsSameOrWithin(left, right) || IsSameOrWithin(right, left);
}

}  // namespace

Result<std::filesystem::path> ResolveIsolatedTestSave(
    const std::filesystem::path& test_root,
    const std::filesystem::path& test_save,
    const std::filesystem::path& game_executable) {
    const auto candidate = ResolveContainedExistingFile(test_root, test_save);
    if (!candidate || _wcsicmp(candidate.value.extension().c_str(), L".sav") != 0) {
        return {{}, ErrorCode::WriteConflict,
            "an isolated contained .sav test copy is required"};
    }

    std::error_code error;
    const auto canonical_root = std::filesystem::weakly_canonical(test_root, error);
    const auto canonical_game_root = error
        ? std::filesystem::path{}
        : std::filesystem::weakly_canonical(
            game_executable.parent_path(), error);
    const auto player_root = PlayerSaveRoot();
    const auto canonical_player_root = error || player_root.empty()
        ? std::filesystem::path{}
        : std::filesystem::weakly_canonical(player_root, error);
    if (error || canonical_game_root.empty() || canonical_player_root.empty() ||
        Overlaps(canonical_root, canonical_game_root) ||
        Overlaps(canonical_root, canonical_player_root)) {
        return {{}, ErrorCode::WriteConflict,
            "test root must be outside the game and player save directories"};
    }
    return candidate;
}

}  // namespace autocattery::save_safety
