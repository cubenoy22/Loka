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

int main()
{
  testStateNotify();
  testDerivedStateCore();
  testConditionalDefinitionCloneOwnership();
  testOwnedDefOwnership();
  testNestableDefinitionCloneReturnsNullOnOomChildClone();
  testNestableDefinitionAssignmentPreservesStableChildOnOomClone();
  testCompositionSnapshotClearsStaleRootOnOomClone();
  testNodeCompositionSkipsOomClones();
  testWindowPropsAssignmentPreservesOwnedSceneOnOomClone();
  testWindowDefinitionCreateReturnsNullOnOomRootClone();
  testWindowDefinitionCreateTransfersSingleRootClone();
  testMenuControllerPreservesDefaultMenuBarOnOomClone();
  testMenuControllerPreservesRefreshedMenuBarOnOomClone();
  testMenuControllerRequeuesDirtyMenusAfterOomClone();
  testMenuControllerSchedulesRetryAfterDirectRefreshFailure();
  testWindowDefinitionTransfersSceneOwnershipToWindow();
  testWindowPropsSceneHandoffIsOneShotAcrossCopies();
  testWindowRetiresDetachedSceneAtFlushBoundary();
  testWindowDoesNotReclaimSceneDuringDetachNotification();
  testLokaValueCore();
  testSnapFormatV1();
  testSnapFlowWriteAdapter();
  testLokaFlowDslV1Core();
  testLokaAttrDslV1Core();
  testPlatformNodeHandlerRegistration();
  testPlatformNodeHandlerReplacement();
  testPlatformNodeHandlerRejectsInvalidTypeKey();
  testPlatformLayoutHandlerRegistration();
  testPlatformLayoutTraversalResultY();
  testPlatformLayoutHandlerReplacement();
  testPlatformLayoutHandlerRejectsInvalidTypeKey();
  testPlatformLayoutHandlerSamePointerReregister();
  testBoundaryArenaContracts();
  SceneTests::runAll();
  testPrepareProjectedLayoutDelegation();
  testProjectedLayoutUsesActiveBoundaryModel();
  testPrepareProjectedLayoutRejectsNullController();
  testContainerLayoutHelpersAdvanceResultY();
  testStartupRedrawCount_Before();
  testStartupRedrawCount_After();
  testToolboxChildDirtyInvalidationPrefersFullRedraw();
  testToolboxManualInvalidateDoesNotSkipFollowupUpdateDraw();
  return 0;
}
