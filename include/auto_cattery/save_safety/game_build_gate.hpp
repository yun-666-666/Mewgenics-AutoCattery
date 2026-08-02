#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "auto_cattery/error.hpp"

namespace autocattery::save_safety {

struct GameBuildFingerprint {
    std::wstring executable_name;
    std::uintmax_t byte_size{};
    std::string sha256;
    std::string identity;
};

class IGameBuildGate {
public:
    virtual ~IGameBuildGate() = default;
    [[nodiscard]] virtual Result<std::string> Verify(
        const std::filesystem::path& executable) const = 0;
};

class ExactGameBuildGate final : public IGameBuildGate {
public:
    explicit ExactGameBuildGate(GameBuildFingerprint expected);

    [[nodiscard]] Result<std::string> Verify(
        const std::filesystem::path& executable) const override;

private:
    GameBuildFingerprint expected_;
};

// Runtime adapters are build-sensitive, but the MOD must not refuse to load
// solely because a new game executable has a different fingerprint. Native
// calls still validate their own pointers/signatures and fail closed.
class MewgenicsExecutableGate final : public IGameBuildGate {
public:
    [[nodiscard]] Result<std::string> Verify(
        const std::filesystem::path& executable) const override;
};

[[nodiscard]] MewgenicsExecutableGate CurrentMewgenicsBuildGate();

}  // namespace autocattery::save_safety
