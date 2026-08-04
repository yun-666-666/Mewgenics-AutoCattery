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
    constexpr std::array<std::size_t, 3> counts{17, 18, 13};
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

}  // namespace autocattery::ui
