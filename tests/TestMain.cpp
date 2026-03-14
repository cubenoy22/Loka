#include "SceneTests.hpp"
#include "StateNotifyTests.hpp"
#include "FlowDslTests.hpp"
#include "AttrDslTests.hpp"
#include "SnapFormatTests.hpp"
#include "Tests.hpp"
#if defined(_WIN32) || defined(WIN32)
#include "Win32ScenePlatformTests.hpp"
#endif
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
#if defined(_WIN32) || defined(WIN32)
  testWin32ScenePlatformRelayoutReusesControlContexts();
  testWin32ScenePlatformRelayoutReusesCellAndTextContexts();
  testWin32ScenePlatformFullRebuildFlagControlsContextReuse();
#endif
#ifdef __APPLE__
  testMacScenePlatformRelayoutRequest();
  testMacScenePlatformIgnoresNonLayoutDirtyRequest();
  testMacScenePlatformRelayoutReusesControlContexts();
  testMacScenePlatformRelayoutReusesCellAndTextContexts();
  testMacScenePlatformFullRebuildFlagControlsContextReuse();
#endif
  testStateBatchOverflow();
  SceneTests::runAll();
  return 0;
}
