#pragma once

#include <filesystem>
#include <string>

namespace autocattery {

struct InitContext {
    std::filesystem::path game_root;
    std::filesystem::path mod_root;
    std::string game_build_id{"unknown"};
};

enum class ModMode {
    ReadOnly,
    Full,
    CompatibilityDegraded
};

class Module {
public:
    virtual ~Module() = default;
    [[nodiscard]] virtual const char* Name() const noexcept = 0;
    virtual bool Initialize(const InitContext& context) = 0;
    virtual void Shutdown() noexcept = 0;
};

}  // namespace autocattery
