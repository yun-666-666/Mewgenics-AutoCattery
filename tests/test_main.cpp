#include "test_support.hpp"

namespace autocattery::tests {

void RunConfigTests();
void RunHouseButtonControllerTests();
void RunModuleRegistryTests();
void RunRecommendationMarkerControllerTests();
void RunSceneContextTests();

}  // namespace autocattery::tests

int main() {
    autocattery::tests::RunConfigTests();
    autocattery::tests::RunHouseButtonControllerTests();
    autocattery::tests::RunModuleRegistryTests();
    autocattery::tests::RunRecommendationMarkerControllerTests();
    autocattery::tests::RunSceneContextTests();
    if (autocattery::tests::failures != 0) {
        return 1;
    }
    return 0;
}
