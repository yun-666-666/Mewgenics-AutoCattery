#include "auto_cattery/save_safety/file_hash.hpp"

#include <chrono>
#include <fstream>

#include "test_support.hpp"

namespace autocattery::tests {

void RunFileHashTests() {
    const auto text = save_safety::Sha256Text("abc");
    AC_CHECK(static_cast<bool>(text));
    AC_CHECK(text.value ==
        "ba7816bf8f01cfea414140de5dae2223"
        "b00361a396177a9cb410ff61f20015ad");

    const auto file = std::filesystem::temp_directory_path() /
        ("auto-cattery-hash-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    {
        std::ofstream output(file, std::ios::binary);
        output << "abc";
    }
    const auto file_hash = save_safety::Sha256File(file);
    AC_CHECK(static_cast<bool>(file_hash));
    AC_CHECK(file_hash.value == text.value);
    std::filesystem::remove(file);
}

}  // namespace autocattery::tests
