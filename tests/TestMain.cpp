#include "SceneTests.hpp"
#include "StateNotifyTests.hpp"
#include "FlowDslTests.hpp"
#include "AttrDslTests.hpp"
#include "SnapFormatTests.hpp"
#include "Tests.hpp"
#include "StartupRedrawTests.hpp"
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
  testNodeDefinitionTagPropagatesToCreatedNodes();
  testNodeCompositionDiffTracksEntries();
  testNodeCompositionTransactionTracksWorkingSet();
  testBuildNodeCompositionDiffByTagTracksRetainReplaceRetire();
  testNodeDefinitionsReportCompatibleLiveNodeKinds();
  testCompositionListReplacePreservesOrder();
  testNodeCompositionSnapshotOwnsClonedRoot();
  testNodeCompositionSnapshotRetainsOwnedPropsSafely();
  testBoundaryCompositionStateStoresSnapshotsLocally();
  testBoundaryComposePathsPopulateTransactions();
  testBoundaryReportsLocalCompositionDiffAvailability();
  testBoundaryRejectsLocalDiffWhenRetainedTypeChanges();
  testDynamicBoundarySkipsRebuildForStableRetainOnlyDiff();
  testBoundaryCanFindLiveCompositionChildByTag();
  testDynamicBoundaryAppliesLocalPropsForCompatibleRetainedChild();
  testDynamicBoundaryLocallyReplacesTaggedChild();
  testDynamicBoundaryLocallyAttachesAndRetiresTaggedChild();
  testDynamicBoundaryLocallyReordersTaggedChildren();
  testDynamicBoundaryLocalReplacePreservesRetainedControlNode();
  testDynamicBoundaryLocalDiffReleasesRetiredSubtrees();
  testDynamicBoundaryLocalDiffPreservesRetainedChildBoundary();
  testDynamicBoundaryLocalDiffHandlesMixedRetainReplaceAndReorder();
  testDynamicBoundaryLocalDiffHandlesMixedPropsReplaceAndReorder();
  testSceneDowngradesNoOpChildDirtyForLocalDiff();
  testSceneKeepsLocalDiffForReplaceableMixedLocalDiff();
  testDynamicBoundaryRepeatedLocalDiffDoesNotGrowChildren();
  testFrozenBoundaryIgnoresObservedStateInvalidation();
  testFrozenBoundaryIgnoresOwnedStateInvalidation();
  testSceneMountLifecycle();
  testSceneBoundaryNestedCompose();
  testStaticBoundaryPropagatesUpdateToDynamicChild();
  testDynamicBoundaryRecomposesOnlyOnChildDirty();
  testDynamicBoundaryPreservesChildIdentityUntilChildDirty();
  testDynamicBoundaryDetachesSubtreeBeforeChildRecompose();
  testDynamicBoundaryRecomposeDoesNotDuplicateBoundaryCallbacks();
  testBoundaryDirtyPolicyStaticImmediateDynamicDeferred();
  testDynamicBoundaryObservedParentOwnedStateTriggersChildRecompose();
  testDynamicBoundaryObservedParentOwnedStateSwapsSampleLikeBranch();
  testPopupMenuObservedStatesInvalidateSceneProps();
  testOpenFileDialogStatesDoNotInvalidateScene();
  testSceneInvalidateUsesRequestedDirtyFlags();
  testSceneRequestInvalidateDefersUntilFlush();
  testSceneCompositionDiffMarksChildDirtyAsFullRebuild();
  testSceneMixedStaticAndDynamicChildDirtyStaysFullRebuild();
  testSceneMixedStaticAndDynamicPureChildDirtyStaysFullRebuild();
  testSceneMixedStaticAndDynamicChildDirtyTracksBoundaryLocalDiffState();
  testSceneMixedDynamicRootChildDirtyDowngradesFullRebuild();
  testSceneMixedDynamicRootPureChildDirtyDowngradesFullRebuild();
  testWindowFlushSceneInvalidationSynchronizesPendingPlatformWork();
  testDebugStatsControlDeferredDumpCompletionCanChainAnotherDump();
  testStaticButtonAndCellTextAreOwnedPerDefinition();
  testMenuItemEnabledBoolDoesNotUseSharedStaticState();
  testConditionalNodeTeardownAfterOwnedStateIsSafe();
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
  testWin32ScenePlatformForeignObservedChildRebuildSwapsContexts();
  testWin32ScenePlatformForeignObservedChildRebuildPreservesSiblingContexts();
  testWin32ScenePlatformForeignObservedChildReorderPreservesSiblingContexts();
#endif
#ifdef __APPLE__
  testMacScenePlatformRelayoutRequest();
  testMacScenePlatformIgnoresNonLayoutDirtyRequest();
  testMacScenePlatformDynamicPropsAndLayoutReuseContexts();
  testMacScenePlatformRelayoutReusesControlContexts();
  testMacScenePlatformRelayoutReusesCellAndTextContexts();
  testMacScenePlatformFullRebuildFlagControlsContextReuse();
  testMacScenePlatformChildRebuildCleansUpOldContexts();
  testMacScenePlatformForeignObservedChildRebuildSwapsContexts();
  testMacScenePlatformForeignObservedChildRebuildPreservesSiblingContexts();
  testMacScenePlatformForeignObservedChildReorderPreservesSiblingContexts();
  testMacScenePlatformHelloWorldCapturesTextAndButtons();
#endif
  testStateBatchOverflow();
  testStartupRedrawCount_Before();
  testStartupRedrawCount_After();
  testDynamicRootMountProducesExactlyOneFullRebuildOnChange();
  SceneTests::runAll();
  return 0;
}
