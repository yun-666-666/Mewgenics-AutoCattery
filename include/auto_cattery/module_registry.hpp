#pragma once

#include <memory>
#include <vector>

#include "auto_cattery/api_types.hpp"
#include "auto_cattery/error.hpp"

namespace autocattery {

class ModuleRegistry {
public:
    Result<void> Register(std::unique_ptr<Module> module);
    Result<void> InitializeAll(const InitContext& context);
    void ShutdownAll() noexcept;
    [[nodiscard]] std::size_t Size() const noexcept;

private:
    std::vector<std::unique_ptr<Module>> modules_;
    std::size_t initialized_count_{};
};

}  // namespace autocattery
