#include "auto_cattery/save_safety/game_build_gate.hpp"

#include <windows.h>

#include "auto_cattery/save_safety/file_hash.hpp"

namespace autocattery::save_safety {

ExactGameBuildGate::ExactGameBuildGate(GameBuildFingerprint expected)
    : expected_(std::move(expected)) {}

Result<std::string> ExactGameBuildGate::Verify(
    const std::filesystem::path& executable) const {
    std::error_code error;
    const auto canonical = std::filesystem::weakly_canonical(executable, error);
    const DWORD attributes = error
        ? INVALID_FILE_ATTRIBUTES
        : GetFileAttributesW(canonical.c_str());
    if (error || attributes == INVALID_FILE_ATTRIBUTES ||
        (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
        (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 ||
        _wcsicmp(canonical.filename().c_str(), expected_.executable_name.c_str()) != 0 ||
        std::filesystem::file_size(canonical, error) != expected_.byte_size ||
        error) {
        return {{}, ErrorCode::UnsupportedGameBuild,
            "the game executable does not match the gated build"};
    }
    const auto hash = Sha256File(canonical);
    if (!hash || _stricmp(hash.value.c_str(), expected_.sha256.c_str()) != 0) {
        return {{}, ErrorCode::UnsupportedGameBuild,
            "the game executable hash does not match the gated build"};
    }
    return {expected_.identity};
}

Result<std::string> MewgenicsExecutableGate::Verify(
    const std::filesystem::path& executable) const {
    std::error_code error;
    const auto canonical = std::filesystem::weakly_canonical(executable, error);
    const DWORD attributes = error
        ? INVALID_FILE_ATTRIBUTES
        : GetFileAttributesW(canonical.c_str());
    if (error || attributes == INVALID_FILE_ATTRIBUTES ||
        (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
        (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 ||
        _wcsicmp(canonical.filename().c_str(), L"Mewgenics.exe") != 0 ||
        !std::filesystem::is_regular_file(canonical, error) || error) {
        return {{}, ErrorCode::UnsupportedGameBuild,
            "a regular Mewgenics.exe file is required"};
    }

    // Do not hash or compare against a pinned build here. The native adapters
    // perform pointer/signature checks at each call and report a safe failure
    // when a future executable no longer matches their verified layout.
    return {"mewgenics-compatible-executable"};
}

MewgenicsExecutableGate CurrentMewgenicsBuildGate() {
    return {};
}

}  // namespace autocattery::save_safety
