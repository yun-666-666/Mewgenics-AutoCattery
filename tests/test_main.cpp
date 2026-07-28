#include "test_support.hpp"

namespace autocattery::tests {

void RunConfigTests();
void RunModuleRegistryTests();
void RunSceneContextTests();

}  // namespace autocattery::tests

int main() {
    autocattery::tests::RunConfigTests();
    autocattery::tests::RunModuleRegistryTests();
    autocattery::tests::RunSceneContextTests();
    if (autocattery::tests::failures != 0) {
        return 1;
    }
    return 0;
}
