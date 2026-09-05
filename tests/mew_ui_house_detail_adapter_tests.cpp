#include "mew_ui_house_detail_adapter.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

struct DetailFixture {
    std::size_t open;
    std::size_t target;
    std::size_t drawer;
    std::size_t cat_call;
    std::size_t drawer_call;
    std::size_t open_call;
};

void WriteRelativeCall(
    std::vector<std::uint8_t>& image,
    std::size_t call,
    std::size_t target) {
    image[call] = 0xE8;
    const auto displacement = static_cast<std::int32_t>(
        target - (call + 5U));
    std::memcpy(image.data() + call + 1U,
                &displacement,
                sizeof(displacement));
}

void InstallDetailFixture(
    std::vector<std::uint8_t>& image,
    const DetailFixture& fixture) {
    constexpr std::array<std::uint8_t, 12> open_signature{
        0x40, 0x53, 0x48, 0x83, 0xEC, 0x40,
        0x48, 0x8B, 0xD9, 0x48, 0x85, 0xD2
    };
    constexpr std::array<std::uint8_t, 19> target_signature{
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89,
        0x74, 0x24, 0x10, 0x57, 0x48, 0x83, 0xEC,
        0x20, 0x48, 0x8B, 0x71, 0x18
    };
    constexpr std::array<std::uint8_t, 25> drawer_signature{
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83,
        0xEC, 0x20, 0x48, 0x8B, 0x79, 0x18, 0xBA, 0xD5,
        0x01, 0x00, 0x00, 0x48, 0x8B, 0x5F, 0x08, 0x48, 0x8B
    };
    constexpr std::array<std::uint8_t, 4> cat_prefix{
        0x49, 0x8B, 0xCF, 0xE8
    };
    constexpr std::array<std::uint8_t, 3> cat_suffix{
        0x4C, 0x8B, 0xF8
    };
    constexpr std::array<std::uint8_t, 4> drawer_prefix{
        0x49, 0x8B, 0xCE, 0xE8
    };
    constexpr std::array<std::uint8_t, 8> drawer_suffix{
        0x48, 0x8B, 0xF8, 0x49, 0x83, 0x7E, 0x38, 0x00
    };
    constexpr std::array<std::uint8_t, 10> open_prefix{
        0x41, 0xB0, 0x01, 0x49, 0x8B,
        0xD7, 0x48, 0x8B, 0xCF, 0xE8
    };
    std::copy(open_signature.begin(), open_signature.end(),
              image.begin() + fixture.open);
    std::copy(target_signature.begin(), target_signature.end(),
              image.begin() + fixture.target);
    std::copy(drawer_signature.begin(), drawer_signature.end(),
              image.begin() + fixture.drawer);
    std::copy(cat_prefix.begin(), cat_prefix.end(),
              image.begin() + fixture.cat_call);
    WriteRelativeCall(image, fixture.cat_call + 3U, fixture.target);
    std::copy(cat_suffix.begin(), cat_suffix.end(),
              image.begin() + fixture.cat_call + 8U);
    std::copy(drawer_prefix.begin(), drawer_prefix.end(),
              image.begin() + fixture.drawer_call);
    WriteRelativeCall(image, fixture.drawer_call + 3U, fixture.drawer);
    std::copy(drawer_suffix.begin(), drawer_suffix.end(),
              image.begin() + fixture.drawer_call + 8U);
    std::copy(open_prefix.begin(), open_prefix.end(),
              image.begin() + fixture.open_call);
    WriteRelativeCall(image, fixture.open_call + 9U, fixture.open);
}

}  // namespace

void RunMewUiHouseDetailAdapterTests() {
    constexpr DetailFixture stable{
        0x00EBEF0U, 0x00EFCB0U, 0x01A93F0U,
        0x001FE082U, 0x001FE262U, 0x001FE2D5U
    };
    constexpr DetailFixture beta{
        0x00EC7B0U, 0x00F0570U, 0x01A9E10U,
        0x001FEAF2U, 0x001FECD2U, 0x001FED45U
    };
    std::vector<std::uint8_t> image(beta.open_call + 0x80U);
    AcMewHouseDetailLayout selected{};
    InstallDetailFixture(image, stable);
    AC_CHECK(AcMewSelectHouseDetailLayout(
        image.data(), image.size(), &selected));
    AC_CHECK(selected.open_cat_details_rva == stable.open);
    std::fill(image.begin(), image.end(), std::uint8_t{0});
    InstallDetailFixture(image, beta);
    AC_CHECK(AcMewSelectHouseDetailLayout(
        image.data(), image.size(), &selected));
    AC_CHECK(selected.cat_detail_target_rva == beta.target);
    image[beta.open] = 0U;
    AC_CHECK(!AcMewSelectHouseDetailLayout(
        image.data(), image.size(), &selected));
}

}  // namespace autocattery::tests
