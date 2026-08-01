#include "test_support.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>

#include <windows.h>

namespace autocattery::tests {

void RunConfigTests();
void RunExecutionPreconditionTests();
void RunExecutionStorageTests();
void RunExecutionTransactionSuccessTests();
void RunExecutionTransactionFailureTests();
void RunExecutionTransactionConcurrencyTests();
void RunFileHashTests();
void RunGameBuildGateTests();
void RunBalancedMoveOnlyPlannerTests();
void RunBreedingRankerTests();
void RunBreedingScorerTests();
void RunBackupManifestTests();
void RunAtomicFileReplaceTests();
void RunClassifierTests();
void RunCombatRankerTests();
void RunCombatRankingCacheTests();
void RunCombatScorerTests();
void RunConfigBoundaryTests();
void RunConfigMigrationTests();
void RunConfigRuntimeTests();
void RunCatBlobParserTests();
void RunHouseButtonControllerTests();
void RunHouseStateParserTests();
void RunHouseStateWriterTests();
void RunHouseMoveProbeSessionTests();
void RunLz4BlockTests();
void RunMewUiHouseCatProbeTests();
void RunMewUiHouseMoveProbeTests();
void RunModuleRegistryTests();
void RunOrganizeWorkflowFacadeTests();
void RunProtectionPolicyTests();
void RunProtectionSidecarTests();
void RunRecommendationMappingProbeTests();
void RunRecommendationRankingProviderTests();
void RunRecommendationSnapshotReaderTests();
void RunRecommendationSnapshotValidatorTests();
void RunRoomCapabilityAdapterTests();
void RunRestoreServiceTests();
void RunSingleCatMoveTestServiceTests();
void RunRoomPlannerTests();
void RunRoomPlanningValidatorTests();
void RunRecommendationMarkerControllerTests();
void RunSaveDatabaseTests();
void RunSaveLocatorTests();
void RunSaveStabilityTests();
void RunSceneContextTests();
void RunSettingsFileEditorTests();
void RunTestCopyGuardTests();
void RunTestCopyHouseStateStoreTests();
void RunSnapshotDomainTests();
void RunSnapshotAssemblerTests();
void RunWinSqliteApiTests();
void RunWorkflowStateMachineTests();
void RunWorkflowDigestTests();
void RunWorkflowPreviewBuilderTests();
void RunWorkflowPreviewStoreTests();
void RunWorkflowPreviewBindingTests();
void RunWorkflowExecutionRouterTests();
void RunWorkflowRecommendationWriterTests();

}  // namespace autocattery::tests

int main() {
    SetErrorMode(
        SEM_FAILCRITICALERRORS |
        SEM_NOGPFAULTERRORBOX |
        SEM_NOOPENFILEERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    try {
    autocattery::tests::RunBalancedMoveOnlyPlannerTests();
    autocattery::tests::RunBreedingRankerTests();
    autocattery::tests::RunBreedingScorerTests();
    autocattery::tests::RunBackupManifestTests();
    autocattery::tests::RunAtomicFileReplaceTests();
    autocattery::tests::RunClassifierTests();
    autocattery::tests::RunCombatRankerTests();
    autocattery::tests::RunCombatRankingCacheTests();
    autocattery::tests::RunCombatScorerTests();
    autocattery::tests::RunConfigBoundaryTests();
    autocattery::tests::RunConfigMigrationTests();
    autocattery::tests::RunConfigRuntimeTests();
    autocattery::tests::RunConfigTests();
    autocattery::tests::RunExecutionPreconditionTests();
    autocattery::tests::RunExecutionStorageTests();
    autocattery::tests::RunExecutionTransactionSuccessTests();
    autocattery::tests::RunExecutionTransactionFailureTests();
    autocattery::tests::RunExecutionTransactionConcurrencyTests();
    autocattery::tests::RunFileHashTests();
    autocattery::tests::RunGameBuildGateTests();
    autocattery::tests::RunCatBlobParserTests();
    autocattery::tests::RunHouseButtonControllerTests();
    autocattery::tests::RunHouseStateParserTests();
    autocattery::tests::RunHouseStateWriterTests();
    autocattery::tests::RunHouseMoveProbeSessionTests();
    autocattery::tests::RunLz4BlockTests();
    autocattery::tests::RunMewUiHouseCatProbeTests();
    autocattery::tests::RunMewUiHouseMoveProbeTests();
    autocattery::tests::RunModuleRegistryTests();
    autocattery::tests::RunOrganizeWorkflowFacadeTests();
    autocattery::tests::RunProtectionPolicyTests();
    autocattery::tests::RunProtectionSidecarTests();
    autocattery::tests::RunRecommendationMappingProbeTests();
    autocattery::tests::RunRecommendationRankingProviderTests();
    autocattery::tests::RunRecommendationSnapshotReaderTests();
    autocattery::tests::RunRecommendationSnapshotValidatorTests();
    autocattery::tests::RunRoomCapabilityAdapterTests();
    autocattery::tests::RunRestoreServiceTests();
    autocattery::tests::RunSingleCatMoveTestServiceTests();
    autocattery::tests::RunRoomPlannerTests();
    autocattery::tests::RunRoomPlanningValidatorTests();
    autocattery::tests::RunRecommendationMarkerControllerTests();
    autocattery::tests::RunSaveDatabaseTests();
    autocattery::tests::RunSaveLocatorTests();
    autocattery::tests::RunSaveStabilityTests();
    autocattery::tests::RunSceneContextTests();
    autocattery::tests::RunSettingsFileEditorTests();
    autocattery::tests::RunTestCopyGuardTests();
    autocattery::tests::RunTestCopyHouseStateStoreTests();
    autocattery::tests::RunSnapshotDomainTests();
    autocattery::tests::RunSnapshotAssemblerTests();
    autocattery::tests::RunWinSqliteApiTests();
    autocattery::tests::RunWorkflowStateMachineTests();
    autocattery::tests::RunWorkflowDigestTests();
    autocattery::tests::RunWorkflowPreviewBuilderTests();
    autocattery::tests::RunWorkflowPreviewStoreTests();
    autocattery::tests::RunWorkflowPreviewBindingTests();
    autocattery::tests::RunWorkflowExecutionRouterTests();
    autocattery::tests::RunWorkflowRecommendationWriterTests();
    } catch (const std::exception& error) {
        std::cerr << "UNHANDLED TEST EXCEPTION: " << error.what() << '\n';
        return 2;
    } catch (...) {
        std::cerr << "UNHANDLED NON-STANDARD TEST EXCEPTION\n";
        return 3;
    }
    if (autocattery::tests::failures != 0) {
        return 1;
    }
    return 0;
}
