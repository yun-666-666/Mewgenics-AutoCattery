#pragma once

#include <string_view>

namespace autocattery {

inline constexpr std::string_view kModName{"AutoCattery"};
inline constexpr std::string_view kModVersion{AUTOCATTERY_VERSION};
inline constexpr int kSupportedConfigSchema = 1;

}  // namespace autocattery
