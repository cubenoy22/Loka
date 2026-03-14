#include "SceneTests.hpp"
#include "StateNotifyTests.hpp"
#include "FlowDslTests.hpp"
#include "AttrDslTests.hpp"
#include "SnapFormatTests.hpp"
#include "Tests.hpp"
#ifdef __APPLE__
#include "MacScenePlatformTests.hpp"
#endif

int main()
{
  testStateNotify();
  testDependencyPropagationCases();
  testTrackerPropagation();
  testDeferredSideEffect();
  testBatchTransaction();
  testRAIITransaction();
  testNestedTransaction();
  testNestedTransactionInvalidateTiming();
  testTextInputOnChange();
  testDerivedStruct();
  testSceneManagerTransaction();
  testNodeCompositionTree();
  testNodeCompositionShowIf();
  testSceneMountLifecycle();
  testSceneBoundaryNestedCompose();
  testStaticBoundaryPropagatesUpdateToDynamicChild();
  testDynamicBoundaryRecomposesOnlyOnChildDirty();
  testDynamicBoundaryDetachesSubtreeBeforeChildRecompose();
  testDynamicBoundaryRecomposeDoesNotDuplicateBoundaryCallbacks();
  testBoundaryDirtyPolicyStaticImmediateDynamicDeferred();
  testPopupMenuSelectionStateDoesNotInvalidateScene();
  testOpenFileDialogStatesDoNotInvalidateScene();
  testSceneInvalidateUsesRequestedDirtyFlags();
  testSceneRequestInvalidateDefersUntilFlush();
  testSceneCompositionDiffMarksChildDirtyAsFullRebuild();
  testLokaCoreString();
  testLokaCoreCollections();
  testLokaDslStream();
  testSnapFormatV1();
  testSnapFlowWriteAdapter();
  testLokaFlowDslV1Core();
  testLokaAttrDslV1Core();
#ifdef __APPLE__
  testMacScenePlatformRelayoutRequest();
  testMacScenePlatformIgnoresNonLayoutDirtyRequest();
  testMacScenePlatformRelayoutReusesControlContexts();
#endif
  testStateBatchOverflow();
  SceneTests::runAll();
  return 0;
}
