#pragma once

#include <optional>

namespace autocattery::ui {

struct ClientPoint {
    double x{};
    double y{};
};

struct VirtualPoint {
    double x{};
    double y{};
};

// The House SWF uses this fixed stage. Mouse messages already carry client
// coordinates in the same DPI context as the game window, so callers must not
// substitute screen-space cursor coordinates here.
inline std::optional<VirtualPoint> MapClientToVirtualViewport(
    ClientPoint point, double client_width, double client_height) noexcept {
    constexpr double kVirtualWidth = 1280.0;
    constexpr double kVirtualHeight = 720.0;
    if (client_width <= 0.0 || client_height <= 0.0 || point.x < 0.0 ||
        point.y < 0.0 || point.x > client_width || point.y > client_height) {
        return std::nullopt;
    }
    return VirtualPoint{
        point.x * kVirtualWidth / client_width,
        point.y * kVirtualHeight / client_height};
}

}  // namespace autocattery::ui
