#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace autocattery::ui {

struct SettingsRowHit {
    std::size_t row{};
    int direction{};
    bool begin_edit{};
};

inline std::optional<SettingsRowHit> HitTestSettingsRow(
    double x, double y) noexcept {
    constexpr std::array<std::size_t, 3> starts{0, 17, 35};
    constexpr std::array<std::size_t, 3> counts{17, 18, 14};
    for (std::size_t column = 0; column < counts.size(); ++column) {
        for (std::size_t row = 0; row < counts[column]; ++row) {
            const double left = 160.0 + column * 317.0;
            const double top = 188.5 + row * 25.0;
            if (x < left || x > left + 300.0 || y < top || y > top + 25.0)
                continue;
            if (x >= left + 75.0 && x <= left + 225.0)
                return SettingsRowHit{starts[column] + row, 0, true};
            return SettingsRowHit{
                starts[column] + row, x < left + 75.0 ? -1 : 1, false};
        }
    }
    return std::nullopt;
}

inline bool ShouldConsumePanelMessage(
    std::uint32_t message, std::uintptr_t wparam) noexcept {
    constexpr std::uint32_t key_down = 0x0100;
    constexpr std::uint32_t sys_key_down = 0x0104;
    constexpr std::uintptr_t escape = 0x1B;
    constexpr std::uint32_t left_down = 0x0201;
    constexpr std::uint32_t left_up = 0x0202;
    constexpr std::uint32_t right_down = 0x0204;
    constexpr std::uint32_t right_up = 0x0205;
    constexpr std::uint32_t middle_down = 0x0207;
    constexpr std::uint32_t middle_up = 0x0208;
    constexpr std::uint32_t mouse_wheel = 0x020A;
    const bool escape_message =
        (message == key_down || message == sys_key_down) && wparam == escape;
    return escape_message || message == left_down || message == left_up ||
        message == right_down || message == right_up ||
        message == middle_down || message == middle_up ||
        message == mouse_wheel;
}

// Used on the game UI thread, including while the management panel is hidden.
// A captured press owns its repeats and key-up even after delivery is stopped.
class DeliveryCancelInput {
public:
    void Start() noexcept { active_ = true; pending_ = false; }
    void Stop() noexcept { active_ = false; pending_ = false; }
    bool TakeCancel() noexcept {
        const bool result = pending_;
        pending_ = false;
        return result;
    }
    bool Consume(std::uint32_t message, std::uintptr_t key) noexcept {
        const unsigned bit = key == 0x1B ? 1U : key == 0x79 ? 2U : 0U;
        const bool down = message == 0x0100 || message == 0x0104;
        const bool up = message == 0x0101 || message == 0x0105;
        if (!bit || (!down && !up)) return false;
        if (down && (active_ || (captured_ & bit))) {
            if (active_ && !(captured_ & bit)) pending_ = true;
            captured_ |= bit;
            return true;
        }
        if (up && (captured_ & bit)) {
            captured_ &= ~bit;
            return true;
        }
        return false;
    }
private:
    bool active_{};
    bool pending_{};
    unsigned captured_{};
};

}  // namespace autocattery::ui
