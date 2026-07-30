#pragma once

#include <filesystem>

#include "auto_cattery/error.hpp"

namespace autocattery::save_safety {

class IGameProcessProbe {
public:
    virtual ~IGameProcessProbe() = default;
    virtual Result<bool> IsRunning(
        const std::filesystem::path& game_executable) = 0;
};

class WindowsGameProcessProbe final : public IGameProcessProbe {
public:
    Result<bool> IsRunning(
        const std::filesystem::path& game_executable) override;
};

}  // namespace autocattery::save_safety
