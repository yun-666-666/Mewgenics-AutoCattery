#include "auto_cattery/save_safety/test_copy_guard.hpp"

#include <chrono>
#include <fstream>

#include "test_support.hpp"

namespace autocattery::tests {

void RunTestCopyGuardTests() {
    const auto root = std::filesystem::temp_directory_path() /
        ("auto-cattery-test-copy-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto game_root = root / "game";
    const auto test_root = root / "isolated";
    std::filesystem::create_directories(game_root);
    std::filesystem::create_directories(test_root);
    const auto game = game_root / "Mewgenics.exe";
    const auto game_save = game_root / "copy.sav";
    const auto save = test_root / "copy.sav";
    const auto outside_save = root / "outside.sav";
    std::ofstream(game) << "game";
    std::ofstream(game_save) << "game save";
    std::ofstream(save) << "save";
    std::ofstream(outside_save) << "outside save";

    const auto resolved = save_safety::ResolveIsolatedTestSave(
        test_root, save, game);
    AC_CHECK(static_cast<bool>(resolved));
    AC_CHECK(!save_safety::ResolveIsolatedTestSave(
        test_root, game, game));
    AC_CHECK(!save_safety::ResolveIsolatedTestSave(
        root, game_root / "not-a-save.txt", game));
    AC_CHECK(!save_safety::ResolveIsolatedTestSave(
        game_root, game_save, game));
    AC_CHECK(!save_safety::ResolveIsolatedTestSave(
        test_root, outside_save, game));
    std::filesystem::remove_all(root);
}

}  // namespace autocattery::tests
