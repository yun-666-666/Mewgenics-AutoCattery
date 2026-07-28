#include "auto_cattery/ui/mew_ui_bridge.hpp"

#include "auto_cattery/logger.hpp"

namespace autocattery::ui {

const char* MewUiBridge::Name() const noexcept {
    return "MewUiBridge";
}

bool MewUiBridge::Initialize(const InitContext&) {
    available_ = false;
    Logger::Instance().Write(
        LogLevel::Warn,
        Name(),
        "AC1201",
        "No verifiable open-source MewUI API was found; UI is disabled and the mod is compatibility-degraded.");
    return true;
}

void MewUiBridge::Shutdown() noexcept {
    available_ = false;
}

bool MewUiBridge::Available() const noexcept {
    return available_;
}

}  // namespace autocattery::ui
