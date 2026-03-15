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
  testSceneManagerPendingTransactionsAndBindings();
  testSceneLifecyclePublishesDestroy();
  testNodeCompositionTree();
  testNodeCompositionShowIf();
  testSceneMountLifecycle();
  testSceneBoundaryNestedCompose();
  testStaticBoundaryPropagatesUpdateToDynamicChild();
  testDynamicBoundaryRecomposesOnlyOnChildDirty();
  testDynamicBoundaryPreservesChildIdentityUntilChildDirty();
  testDynamicBoundaryDetachesSubtreeBeforeChildRecompose();
  testDynamicBoundaryRecomposeDoesNotDuplicateBoundaryCallbacks();
  testBoundaryDirtyPolicyStaticImmediateDynamicDeferred();
  testPopupMenuSelectionStateDoesNotInvalidateScene();
  testOpenFileDialogStatesDoNotInvalidateScene();
  testSceneInvalidateUsesRequestedDirtyFlags();
  testSceneRequestInvalidateDefersUntilFlush();
  testSceneCompositionDiffMarksChildDirtyAsFullRebuild();
  testWindowFlushSceneInvalidationSynchronizesPendingPlatformWork();
  testStaticButtonAndCellTextAreOwnedPerDefinition();
  testMenuItemEnabledBoolDoesNotUseSharedStaticState();
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
  testWin32ScenePlatformDynamicPropsAndLayoutReuseContexts();
  testWin32ScenePlatformFullRebuildFlagControlsContextReuse();
  testWin32ScenePlatformChildRebuildCleansUpOldContexts();
#endif
#ifdef __APPLE__
  testMacScenePlatformRelayoutRequest();
  testMacScenePlatformIgnoresNonLayoutDirtyRequest();
  testMacScenePlatformDynamicPropsAndLayoutReuseContexts();
  testMacScenePlatformRelayoutReusesControlContexts();
  testMacScenePlatformRelayoutReusesCellAndTextContexts();
  testMacScenePlatformFullRebuildFlagControlsContextReuse();
  testMacScenePlatformChildRebuildCleansUpOldContexts();
#endif
  testStateBatchOverflow();
  SceneTests::runAll();
  return 0;
}
