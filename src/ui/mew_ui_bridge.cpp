#include "auto_cattery/ui/mew_ui_bridge.hpp"

#include "auto_cattery/logger.hpp"
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#include "mew_ui_api.h"

namespace autocattery::ui {

const char* MewUiBridge::Name() const noexcept {
    return "MewUiBridge";
}

bool MewUiBridge::Initialize(const InitContext&) {
    ready_logged_.store(false);
    started_ = MewUI_Start(
        "AutoCattery",
        30,
        100,
        100,
        &MewUiBridge::Tick,
        this) != 0;

    if (started_) {
        Logger::Instance().Write(
            LogLevel::Info,
            Name(),
            "AC1200",
            "MewUI API 1.2.0 bootstrap started; no formal UI is created in phase 01.");
        return true;
    }

    Logger::Instance().Write(
        LogLevel::Error,
        Name(),
        "AC1201",
        "MewUI API bootstrap failed; UI is disabled and the mod is compatibility-degraded.");
    return true;
}

void MewUiBridge::Shutdown() noexcept {
    if (started_) {
        MewUI_Stop();
    }
    started_ = false;
    ready_logged_.store(false);
}

bool MewUiBridge::Available() const noexcept {
    return started_;
}

bool MewUiBridge::Ready() const noexcept {
    return started_ && MewUI_IsReady() != 0;
}

void __cdecl MewUiBridge::Tick(void* user_data) {
    auto* bridge = static_cast<MewUiBridge*>(user_data);
    if (bridge == nullptr || MewUI_IsReady() == 0) {
        return;
    }
    if (!bridge->ready_logged_.exchange(true)) {
        Logger::Instance().Write(
            LogLevel::Info,
            bridge->Name(),
            "AC1202",
            "MewUI API hooks are ready on the game UI thread.");
    }
}

}  // namespace autocattery::ui
