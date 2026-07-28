#pragma once

#include "auto_cattery/api_types.hpp"

namespace autocattery::ui {

class MewUiBridge final : public Module {
public:
    [[nodiscard]] const char* Name() const noexcept override;
    bool Initialize(const InitContext& context) override;
    void Shutdown() noexcept override;
    [[nodiscard]] bool Available() const noexcept;

private:
    bool available_{};
};

}  // namespace autocattery::ui
