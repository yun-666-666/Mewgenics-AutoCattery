#include "auto_cattery/save_safety/atomic_file_replace.hpp"

#include <chrono>
#include <fstream>

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

void Write(const std::filesystem::path& path, std::string_view text) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << text;
}

std::string Read(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), {}};
}

}  // namespace

void RunAtomicFileReplaceTests() {
    const auto root = std::filesystem::temp_directory_path() /
        ("auto-cattery-replace-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    const auto destination = root / "fixture.sav";
    const auto temporary = root / "fixture.sav.tmp";
    Write(destination, "old-content");
    Write(temporary, "new-content");

    save_safety::AtomicFileReplacer replacer;
    const auto replaced = replacer.ReplaceTemporary(temporary, destination);
    AC_CHECK(static_cast<bool>(replaced));
    AC_CHECK(Read(destination) == "new-content");
    AC_CHECK(!std::filesystem::exists(temporary));

    Write(temporary, "will-not-replace");
    AC_CHECK(!replacer.ReplaceTemporary(temporary, root / "missing.sav"));
    AC_CHECK(std::filesystem::exists(temporary));
    std::filesystem::remove_all(root);
}

}  // namespace autocattery::tests
