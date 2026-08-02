#include <cmath>

#include "auto_cattery/ui/virtual_viewport.hpp"
#include "test_support.hpp"

namespace autocattery::tests {

void RunVirtualViewportTests() {
    const auto widescreen = ui::MapClientToVirtualViewport(
        {800.0, 450.0}, 1600.0, 900.0);
    AC_CHECK(widescreen.has_value());
    AC_CHECK(std::abs(widescreen->x - 640.0) < 0.001);
    AC_CHECK(std::abs(widescreen->y - 360.0) < 0.001);

    const auto small_screen = ui::MapClientToVirtualViewport(
        {512.0, 384.0}, 1024.0, 768.0);
    AC_CHECK(small_screen.has_value());
    AC_CHECK(std::abs(small_screen->x - 640.0) < 0.001);
    AC_CHECK(std::abs(small_screen->y - 360.0) < 0.001);

    const auto setting = ui::MapClientToVirtualViewport(
        {720.0, 690.0}, 1920.0, 1080.0);
    AC_CHECK(setting.has_value());
    AC_CHECK(std::abs(setting->x - 480.0) < 0.001);
    AC_CHECK(std::abs(setting->y - 460.0) < 0.001);
    AC_CHECK(!ui::MapClientToVirtualViewport({-1.0, 0.0}, 1280.0, 720.0));
    AC_CHECK(!ui::MapClientToVirtualViewport({1281.0, 0.0}, 1280.0, 720.0));
}

}  // namespace autocattery::tests
