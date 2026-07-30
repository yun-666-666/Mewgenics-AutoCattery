#include "house_move_probe_session.hpp"

#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <string>

#include "test_support.hpp"

namespace autocattery::tests {

void RunHouseMoveProbeSessionTests() {
    std::array<std::uint8_t, AC_MEW_MOVE_PROBE_COMPONENT_BYTES> component{};
    std::array<std::uint8_t, AC_MEW_MOVE_PROBE_ROOT_BYTES> root{};
    constexpr char kBeforeRoom[] = "Floor1_Large";
    constexpr char kAfterRoom[] = "Attic";
    constexpr std::size_t kRoomOffset = 0x300U;
    constexpr std::size_t kCoordinatesOffset = 0x400U;
    const std::array<double, 3> before_coordinates{1.0, 2.0, 3.0};
    const std::array<double, 3> after_coordinates{4.0, 5.0, 6.0};
    std::memcpy(
        component.data() + kRoomOffset,
        kBeforeRoom,
        sizeof(kBeforeRoom));
    std::memcpy(
        component.data() + kCoordinatesOffset,
        before_coordinates.data(),
        sizeof(before_coordinates));

    ui::HouseMoveProbeSession session;
    session.Arm(42, {{777, component.data(), root.data()}});
    AC_CHECK(session.Armed());
    AC_CHECK(session.MatchCount() == 1U);
    AC_CHECK(session.CaptureBefore());
    AC_CHECK(session.HasBeforeSample());

    std::memset(
        component.data() + kRoomOffset,
        0,
        sizeof(kBeforeRoom));
    std::memcpy(
        component.data() + kRoomOffset,
        kAfterRoom,
        sizeof(kAfterRoom));
    std::memcpy(
        component.data() + kCoordinatesOffset,
        after_coordinates.data(),
        sizeof(after_coordinates));
    auto report = session.CaptureAfter();
    AC_CHECK(static_cast<bool>(report));
    AC_CHECK(report.value.scene_generation == 42);
    AC_CHECK(report.value.differences.size() == 1U);
    AC_CHECK(!session.HasBeforeSample());

    const auto directory = std::filesystem::temp_directory_path() /
        ("auto-cattery-move-probe-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto written =
        ui::WriteHouseMoveProbeReport(directory, report.value);
    AC_CHECK(static_cast<bool>(written));
    AC_CHECK(std::filesystem::exists(written.value));
    std::ifstream input(written.value, std::ios::binary);
    const std::string text{
        std::istreambuf_iterator<char>(input), {}};
    AC_CHECK(text.find("\"anonymous_cat_ordinal\": 1") !=
             std::string::npos);
    AC_CHECK(text.find("Floor1_Large") != std::string::npos);
    AC_CHECK(text.find("Attic") != std::string::npos);
    AC_CHECK(text.find("777") == std::string::npos);
    AC_CHECK(text.find("CatId") == std::string::npos);
    input.close();
    std::filesystem::remove_all(directory);

    session.Cancel();
    AC_CHECK(!session.Armed());
}

}  // namespace autocattery::tests
