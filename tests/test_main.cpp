#include "test_support.hpp"

namespace autocattery::tests {

void RunConfigTests();
void RunCombatRankerTests();
void RunCombatRankingCacheTests();
void RunCombatScorerTests();
void RunCatBlobParserTests();
void RunHouseButtonControllerTests();
void RunHouseStateParserTests();
void RunLz4BlockTests();
void RunModuleRegistryTests();
void RunOrganizeWorkflowFacadeTests();
void RunRecommendationMarkerControllerTests();
void RunSaveDatabaseTests();
void RunSaveLocatorTests();
void RunSceneContextTests();
void RunSnapshotDomainTests();
void RunSnapshotAssemblerTests();
void RunWinSqliteApiTests();

}  // namespace autocattery::tests

int main() {
    autocattery::tests::RunCombatRankerTests();
    autocattery::tests::RunCombatRankingCacheTests();
    autocattery::tests::RunCombatScorerTests();
    autocattery::tests::RunConfigTests();
    autocattery::tests::RunCatBlobParserTests();
    autocattery::tests::RunHouseButtonControllerTests();
    autocattery::tests::RunHouseStateParserTests();
    autocattery::tests::RunLz4BlockTests();
    autocattery::tests::RunModuleRegistryTests();
    autocattery::tests::RunOrganizeWorkflowFacadeTests();
    autocattery::tests::RunRecommendationMarkerControllerTests();
    autocattery::tests::RunSaveDatabaseTests();
    autocattery::tests::RunSaveLocatorTests();
    autocattery::tests::RunSceneContextTests();
    autocattery::tests::RunSnapshotDomainTests();
    autocattery::tests::RunSnapshotAssemblerTests();
    autocattery::tests::RunWinSqliteApiTests();
    if (autocattery::tests::failures != 0) {
        return 1;
    }
    return 0;
}
