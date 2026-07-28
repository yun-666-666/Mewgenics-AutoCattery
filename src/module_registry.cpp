#include "auto_cattery/module_registry.hpp"

#include <string_view>

namespace autocattery {

Result<void> ModuleRegistry::Register(std::unique_ptr<Module> module) {
    if (!module) {
        return {ErrorCode::InternalError, "cannot register a null module"};
    }
    for (const auto& existing : modules_) {
        if (std::string_view(existing->Name()) == module->Name()) {
            return {ErrorCode::InternalError, "module name is already registered"};
        }
    }
    modules_.push_back(std::move(module));
    return {};
}

Result<void> ModuleRegistry::InitializeAll(const InitContext& context) {
    initialized_count_ = 0;
    for (auto& module : modules_) {
        if (!module->Initialize(context)) {
            ShutdownAll();
            return {
                ErrorCode::InternalError,
                std::string("module initialization failed: ") + module->Name()
            };
        }
        ++initialized_count_;
    }
    return {};
}

void ModuleRegistry::ShutdownAll() noexcept {
    while (initialized_count_ > 0) {
        --initialized_count_;
        modules_[initialized_count_]->Shutdown();
    }
}

std::size_t ModuleRegistry::Size() const noexcept {
    return modules_.size();
}

}  // namespace autocattery
