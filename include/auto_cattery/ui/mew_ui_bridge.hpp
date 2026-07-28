#pragma once

#include <atomic>

#include "auto_cattery/api_types.hpp"

namespace autocattery::ui {

class MewUiBridge final : public Module {
public:
    [[nodiscard]] const char* Name() const noexcept override;
    bool Initialize(const InitContext& context) override;
    void Shutdown() noexcept override;
    [[nodiscard]] bool Available() const noexcept;
    [[nodiscard]] bool Ready() const noexcept;

private:
    static void __cdecl Tick(void* user_data);

    bool started_{};
    std::atomic_bool ready_logged_{false};
};

}  // namespace autocattery::ui
