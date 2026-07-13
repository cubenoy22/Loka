#include "StateNotifyTests.hpp"
#include "DerivedStateTests.hpp"
#include "DefinitionCloneTests.hpp"
#include "FlowDslTests.hpp"
#include "AttrDslTests.hpp"
#include "SnapFormatTests.hpp"
#include "SceneTests.hpp"
#include "StartupRedrawTests.hpp"
#include "BoundaryArenaTests.hpp"
#include "ValueTests.hpp"
#include "SceneOwnershipTests.hpp"
#include "core/LifecycleAudit.hpp"

#define LOKA_RUN_TEST(testFunction)                                                                                    \
  do                                                                                                                   \
  {                                                                                                                    \
    testFunction();                                                                                                    \
    LOKA_AUDIT_CHECKPOINT(#testFunction);                                                                              \
  } while (false)

int main()
{
  LOKA_RUN_TEST(testStateNotify);
  LOKA_RUN_TEST(testDerivedStateCore);
  LOKA_RUN_TEST(testConditionalDefinitionCloneOwnership);
  LOKA_RUN_TEST(testConditionalDefinitionAssignmentPreservesPairOnSecondCloneFailure);
  LOKA_RUN_TEST(testConditionalDefinitionCloneReturnsNullOnSecondBranchFailure);
  LOKA_RUN_TEST(testConditionalDefinitionCopyDegradesToEmptyOnCloneFailure);
  LOKA_RUN_TEST(testOwnedDefOwnership);
  LOKA_RUN_TEST(testNestableDefinitionCloneReturnsNullOnOomChildClone);
  LOKA_RUN_TEST(testNestableDefinitionAssignmentPreservesStableChildOnOomClone);
  LOKA_RUN_TEST(testCompositionSnapshotClearsStaleRootOnOomClone);
  LOKA_RUN_TEST(testNodeCompositionSkipsOomClones);
  LOKA_RUN_TEST(testWindowPropsAssignmentPreservesOwnedSceneOnOomClone);
  LOKA_RUN_TEST(testWindowDefinitionCreateReturnsNullOnOomRootClone);
  LOKA_RUN_TEST(testWindowDefinitionCreateTransfersSingleRootClone);
  LOKA_RUN_TEST(testMenuControllerPreservesDefaultMenuBarOnOomClone);
  LOKA_RUN_TEST(testMenuControllerPreservesRefreshedMenuBarOnOomClone);
  LOKA_RUN_TEST(testMenuControllerRequeuesDirtyMenusAfterOomClone);
  LOKA_RUN_TEST(testMenuControllerSchedulesRetryAfterDirectRefreshFailure);
  LOKA_RUN_TEST(testWindowDefinitionTransfersSceneOwnershipToWindow);
  LOKA_RUN_TEST(testWindowPropsSceneHandoffIsOneShotAcrossCopies);
  LOKA_RUN_TEST(testWindowRetiresDetachedSceneAtFlushBoundary);
  LOKA_RUN_TEST(testWindowDoesNotReclaimSceneDuringDetachNotification);
  LOKA_RUN_TEST(testLokaValueCore);
  LOKA_RUN_TEST(testSnapFormatV1);
  LOKA_RUN_TEST(testSnapFlowWriteAdapter);
  LOKA_RUN_TEST(testLokaFlowDslV1Core);
  LOKA_RUN_TEST(testLokaAttrDslV1Core);
  LOKA_RUN_TEST(testPlatformNodeHandlerRegistration);
  LOKA_RUN_TEST(testPlatformNodeHandlerReplacement);
  LOKA_RUN_TEST(testPlatformNodeHandlerRejectsInvalidTypeKey);
  LOKA_RUN_TEST(testPlatformLayoutHandlerRegistration);
  LOKA_RUN_TEST(testPlatformLayoutTraversalResultY);
  LOKA_RUN_TEST(testPlatformLayoutHandlerReplacement);
  LOKA_RUN_TEST(testPlatformLayoutHandlerRejectsInvalidTypeKey);
  LOKA_RUN_TEST(testPlatformLayoutHandlerSamePointerReregister);
  LOKA_RUN_TEST(testBoundaryArenaContracts);
  LOKA_RUN_TEST(SceneTests::runAll);
  LOKA_RUN_TEST(testPrepareProjectedLayoutDelegation);
  LOKA_RUN_TEST(testProjectedLayoutUsesActiveBoundaryModel);
  LOKA_RUN_TEST(testPrepareProjectedLayoutRejectsNullController);
  LOKA_RUN_TEST(testContainerLayoutHelpersAdvanceResultY);
  LOKA_RUN_TEST(testStartupRedrawCount_Before);
  LOKA_RUN_TEST(testStartupRedrawCount_After);
  LOKA_RUN_TEST(testToolboxChildDirtyInvalidationPrefersFullRedraw);
  LOKA_RUN_TEST(testToolboxManualInvalidateDoesNotSkipFollowupUpdateDraw);
  LOKA_AUDIT_CHECKPOINT("FlowDslTestMain final");
  return 0;
}

#undef LOKA_RUN_TEST
