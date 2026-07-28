#pragma once

#include <iostream>
#include <string_view>

namespace autocattery::tests {

inline int failures = 0;

inline void Check(bool condition, std::string_view expression, int line) {
    if (!condition) {
        std::cerr << "FAILED line " << line << ": " << expression << '\n';
        ++failures;
    }
}

}  // namespace autocattery::tests

#define AC_CHECK(expression) \
    ::autocattery::tests::Check((expression), #expression, __LINE__)
