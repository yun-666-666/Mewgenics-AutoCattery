#include <cmath>

#include "auto_cattery/ui/virtual_viewport.hpp"
#include "auto_cattery/ui/management_panel_input.hpp"
#include "test_support.hpp"

namespace autocattery::tests {

void RunVirtualViewportTests() {
    const auto widescreen = ui::MapClientToVirtualViewport(
        {800.0, 450.0}, 1600.0, 900.0);
    AC_CHECK(widescreen.has_value());
    if (widescreen) {
        AC_CHECK(std::abs(widescreen->x - 640.0) < 0.001);
        AC_CHECK(std::abs(widescreen->y - 360.0) < 0.001);
    }

    const auto small_screen = ui::MapClientToVirtualViewport(
        {512.0, 384.0}, 1024.0, 768.0);
    AC_CHECK(small_screen.has_value());
    if (small_screen) {
        AC_CHECK(std::abs(small_screen->x - 640.0) < 0.001);
        AC_CHECK(std::abs(small_screen->y - 360.0) < 0.001);
    }

    const auto setting = ui::MapClientToVirtualViewport(
        {720.0, 690.0}, 1920.0, 1080.0);
    AC_CHECK(setting.has_value());
    if (setting) {
        AC_CHECK(std::abs(setting->x - 480.0) < 0.001);
        AC_CHECK(std::abs(setting->y - 460.0) < 0.001);
    }
    AC_CHECK(!ui::MapClientToVirtualViewport({-1.0, 0.0}, 1280.0, 720.0));
    AC_CHECK(!ui::MapClientToVirtualViewport({1281.0, 0.0}, 1280.0, 720.0));

    const auto combat_luck = ui::HitTestSettingsRow(310.0, 553.0);
    AC_CHECK(combat_luck.has_value());
    if (combat_luck) {
        AC_CHECK(combat_luck->row == 14);
        AC_CHECK(combat_luck->begin_edit);
    }
    const auto breeding_luck = ui::HitTestSettingsRow(627.0, 503.0);
    AC_CHECK(breeding_luck.has_value());
    if (breeding_luck) {
        AC_CHECK(breeding_luck->row == 29);
        AC_CHECK(breeding_luck->begin_edit);
    }
    const auto reroll_count = ui::HitTestSettingsRow(944.0, 503.0);
    AC_CHECK(reroll_count.has_value());
    if (reroll_count) {
        AC_CHECK(reroll_count->row == 47);
        AC_CHECK(reroll_count->begin_edit);
    }
    const auto population = ui::HitTestSettingsRow(944.0, 528.0);
    AC_CHECK(population.has_value());
    if (population) {
        AC_CHECK(population->row == 48);
        AC_CHECK(population->begin_edit);
    }
    for (const auto x : {810.0, 1070.0}) {
        const auto arrow = ui::HitTestSettingsRow(x, 528.0);
        AC_CHECK(arrow.has_value());
        if (arrow) {
            AC_CHECK(arrow->row == 48);
            AC_CHECK(!arrow->begin_edit);
            AC_CHECK(arrow->direction == (x < 944.0 ? -1 : 1));
        }
    }
    AC_CHECK(!ui::HitTestSettingsRow(944.0, 553.0));
    AC_CHECK(ui::ShouldConsumePanelMessage(0x0100, 0x1B));
    AC_CHECK(ui::ShouldConsumePanelMessage(0x0104, 0x1B));
    AC_CHECK(!ui::ShouldConsumePanelMessage(0x0100, 'A'));

    ui::DeliveryCancelInput delivery_input;
    AC_CHECK(!delivery_input.Consume(0x0100, 0x1B));
    delivery_input.Start();
    AC_CHECK(!delivery_input.Consume(0x0100, 'A'));
    AC_CHECK(!delivery_input.Consume(0x0201, 0));
    AC_CHECK(delivery_input.Consume(0x0100, 0x1B));
    AC_CHECK(delivery_input.TakeCancel());
    AC_CHECK(!delivery_input.TakeCancel());
    delivery_input.Stop();
    // Key repeat and release must not close the native drawer after cancel.
    AC_CHECK(delivery_input.Consume(0x0100, 0x1B));
    AC_CHECK(!delivery_input.TakeCancel());
    AC_CHECK(delivery_input.Consume(0x0101, 0x1B));
    AC_CHECK(!delivery_input.Consume(0x0100, 0x1B));

    delivery_input.Start();
    AC_CHECK(delivery_input.Consume(0x0104, 0x79)); // F10 uses WM_SYSKEYDOWN.
    AC_CHECK(delivery_input.TakeCancel());
    AC_CHECK(!delivery_input.Consume(0x0104, 0x73)); // Alt+F4 is untouched.
    delivery_input.Stop();
    AC_CHECK(delivery_input.Consume(0x0104, 0x79));
    AC_CHECK(delivery_input.Consume(0x0105, 0x79));
    AC_CHECK(!delivery_input.Consume(0x0104, 0x79));

    delivery_input.Start();
    // A short press between polls remains queued, even if released already.
    AC_CHECK(delivery_input.Consume(0x0100, 0x1B));
    AC_CHECK(delivery_input.Consume(0x0101, 0x1B));
    AC_CHECK(delivery_input.TakeCancel());
    delivery_input.Stop();
    AC_CHECK(!delivery_input.Consume(0x0100, 0x1B));
}

}  // namespace autocattery::tests
