#include "mew_breeding_assistance.hpp"

#include <atomic>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <intrin.h>
#include <windows.h>

#include "auto_cattery/breeding/offspring_assistance.hpp"
#include "auto_cattery/logger.hpp"
#include "mewjector.h"

namespace autocattery::ui {
namespace {
// Current build: D8580 registers the pedigree, calls A89A0, sets the child's
// ID at C48 and birthday at C38, then returns the completed CatData pointer.
using Birth = void* (__fastcall *)(void*, std::int64_t, std::int64_t, void*);
Birth next_birth{};
std::atomic<bool> enabled{false};
using RoomEffect = double (__fastcall *)(void*, void*, int, void*);
RoomEffect next_room_effect{};
std::atomic<bool> food_enabled{false};
std::uintptr_t feeding_return{};

double __fastcall AssistedRoomFood(void* effects, void* unused, int category, void* extra) {
    const bool assist = food_enabled.load() && category == 0x1D &&
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()) == feeding_return;
    const double original = next_room_effect(effects, unused, category, extra);
    // Only the end-of-day temporary room ration is changed. Other effect
    // queries, furniture values and persistent house food remain native.
    return assist ? std::max(original, 1000000.0) : original;
}

void* __fastcall AssistedBirth(void* registry, std::int64_t first, std::int64_t second, void* effects) {
    auto* child = static_cast<std::uint8_t*>(next_birth(registry, first, second, effects));
    if (!child || !enabled.load()) return child;
    std::array<std::int32_t, 7> before{};
    std::memcpy(before.data(), child + 0x6F0, sizeof(before));
    auto after = before;
    if (!breeding::ApplyOffspringAssistance(after, true)) return child;
    std::int64_t id{};
    std::memcpy(&id, child + 0xC48, sizeof(id));
    // Write the prior values to the existing local audit log before mutation.
    std::ostringstream audit;
    audit << "offspring=" << id << " parents=" << first << ',' << second << " genetic_before=";
    for (const auto value : before) audit << value << ',';
    audit << " genetic_after=7,7,7,7,7,7,7";
    Logger::Instance().Write(LogLevel::Info, "BreedingAssistance", "AC4101", audit.str());
    std::memcpy(child + 0x6F0, after.data(), sizeof(after));
    return child;
}
}

void InitializeBreedingAssistance(bool supported_build) {
    if (!supported_build) return;
    MewjectorAPI api{};
    if (!MJ_Resolve(&api) || !api.InstallHook) return;
    // Five complete position-independent instructions, inspected in D8580:
    // mov [rsp+10],rbx; mov [rsp+18],rbp; push rsi; push rdi; push r13.
    if (!next_birth && !api.InstallHook(0xD8580, 14, reinterpret_cast<void*>(&AssistedBirth),
                         reinterpret_cast<void**>(&next_birth), 30, "AutoCattery")) {
        Logger::Instance().Write(LogLevel::Warn, "BreedingAssistance", "AC4100",
                                 "Newborn hook unavailable; assistance remains inactive.");
    }
    feeding_return = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)) + 0x1E8F90;
    // 1B5970: three complete stack mov instructions (15 bytes). The caller
    // at 1E8F8B passes category in R8D; its return is ceiled into room+78.
    if (!next_room_effect && !api.InstallHook(0x1B5970, 15,
            reinterpret_cast<void*>(&AssistedRoomFood),
            reinterpret_cast<void**>(&next_room_effect), 30, "AutoCattery")) {
        Logger::Instance().Write(LogLevel::Warn, "BreedingAssistance", "AC4102",
                                 "Temporary room food hook unavailable.");
    }
}

void ConfigureBreedingAssistance(const Config& config) {
    enabled.store(config.room_planning.offspring_all_seven_assist && config.mod_enabled &&
                  !config.safe_mode && !config.force_read_only &&
                  !config.execution_safety.read_only_mode);
    const bool food = config.room_planning.food_supply_assist && config.mod_enabled &&
        !config.safe_mode && !config.force_read_only && !config.execution_safety.read_only_mode;
    if (food_enabled.exchange(food) != food) {
        Logger::Instance().Write(LogLevel::Info, "BreedingAssistance", "AC4103",
            food ? "Food assistance ON: temporary daily room rations at least 1000000."
                 : "Food assistance OFF: native room rations.");
    }
}

void DisableBreedingAssistance() { enabled.store(false); food_enabled.store(false); }
}
