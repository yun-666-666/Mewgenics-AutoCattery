#include "auto_cattery/save_safety/game_build_gate.hpp"

#include <chrono>
#include <fstream>

#include "auto_cattery/save_safety/file_hash.hpp"
#include "test_support.hpp"

namespace autocattery::tests {

void RunGameBuildGateTests() {
    const auto root = std::filesystem::temp_directory_path() /
        ("auto-cattery-build-gate-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    const auto executable = root / "SyntheticGame.exe";
    {
        std::ofstream output(executable, std::ios::binary);
        output << "verified-build";
    }
    const auto hash = save_safety::Sha256File(executable);
    AC_CHECK(static_cast<bool>(hash));
    save_safety::ExactGameBuildGate gate({
        .executable_name = L"SyntheticGame.exe",
        .byte_size = 14,
        .sha256 = hash.value,
        .identity = "synthetic-build"
    });
    const auto verified = gate.Verify(executable);
    AC_CHECK(static_cast<bool>(verified));
    AC_CHECK(verified.value == "synthetic-build");

    save_safety::ExactGameBuildGate wrong_size({
        L"SyntheticGame.exe", 15, hash.value, "wrong"
    });
    AC_CHECK(!wrong_size.Verify(executable));
    save_safety::ExactGameBuildGate wrong_hash({
        L"SyntheticGame.exe", 14, std::string(64, '0'), "wrong"
    });
    AC_CHECK(!wrong_hash.Verify(executable));
    AC_CHECK(!gate.Verify(root / "missing.exe"));
    std::filesystem::remove_all(root);
}

}  // namespace autocattery::tests
