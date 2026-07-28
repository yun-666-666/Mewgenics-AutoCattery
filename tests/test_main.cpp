#include "test_support.hpp"

namespace autocattery::tests {

void RunConfigTests();
void RunModuleRegistryTests();

}  // namespace autocattery::tests

int main() {
    autocattery::tests::RunConfigTests();
    autocattery::tests::RunModuleRegistryTests();
    if (autocattery::tests::failures != 0) {
        return 1;
    }
    return 0;
}
