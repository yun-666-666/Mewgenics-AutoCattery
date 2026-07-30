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

ExactGameBuildGate CurrentMewgenicsBuildGate() {
    return ExactGameBuildGate({
        .executable_name = L"Mewgenics.exe",
        .byte_size = 21'981'184,
        .sha256 =
            "C3A41E436A93FA58CD386EC46DAD5C2A6F21A583D33C3A57A15A2604C726439E",
        .identity =
            "mewgenics-sha256-c3a41e436a93fa58cd386ec46dad5c2a6f21a583d33c3a57a15a2604c726439e"
    });
}

}  // namespace autocattery::save_safety
