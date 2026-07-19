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
#include "LifecycleDetachTests.hpp"
#include "NativeLifetimeTests.hpp"
#include "NullPlatformContractTests.hpp"
#include "LifecycleFactTests.hpp"
#include "core/diag/LifecycleAudit.hpp"

LOKA_DECLARE_LIFECYCLE_AUDIT_TAG(LifecycleAuditedConstructorProbe)
LOKA_DECLARE_LIFECYCLE_AUDIT_TAG(LifecycleAuditedConstructorProbeVerified)

namespace
{
  class LifecycleAuditedConstructorProbe LOKA_AUDITED(LifecycleAuditedConstructorProbe)
  {
  public:
    explicit LifecycleAuditedConstructorProbe(int value)
        : value_(value)
    {
    }

#ifdef LOKA_LIFECYCLE_AUDIT
    void lifecycleAuditReclassify(const char *tag, loka::core::LifecycleAuditDomain domain)
    {
      this->reclassifyLifecycleAudit(tag, domain);
    }
#endif

  private:
    int value_;
  };

  void testLifecycleAuditedCountsEveryConstructor()
  {
    LifecycleAuditedConstructorProbe original(1);
    LifecycleAuditedConstructorProbe copy(original);
    copy = original;
    LOKA_AUDIT_RECLASSIFY_ALIVE(original, LifecycleAuditedConstructorProbeVerified,
                                loka::core::LIFECYCLE_AUDIT_CHAIN_RESIDENT);
    LOKA_AUDIT_RECLASSIFY_ALIVE(copy, LifecycleAuditedConstructorProbeVerified,
                                loka::core::LIFECYCLE_AUDIT_CHAIN_RESIDENT);
  }
} // namespace

#define LOKA_RUN_TEST(testFunction)                                                                                    \
  do                                                                                                                   \
  {                                                                                                                    \
    testFunction();                                                                                                    \
    LOKA_AUDIT_CHECKPOINT(#testFunction);                                                                              \
  } while (false)

int main()
{
  LOKA_RUN_TEST(testLifecycleAuditedCountsEveryConstructor);
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
  LOKA_RUN_TEST(testWindowDefersSceneRetiredDuringDrainUntilNextFlush);
  LOKA_RUN_TEST(testWindowDoesNotReclaimSceneDuringDetachNotification);
  LOKA_RUN_TEST(testWindowReclaimDoesNotNotifySceneLifecycleObservers);
  LOKA_RUN_TEST(testWindowReclaimFiresNoSceneCompositionCallbacks);
  LOKA_RUN_TEST(testAppDefersWindowReclaimUntilInvalidationFlush);
  LOKA_RUN_TEST(testAppDefersReentrantWindowCloseRequestUntilNextFlush);
  LOKA_RUN_TEST(testAppReclaimsWindowCloseBatchInRequestOrder);
  LOKA_RUN_TEST(testAppWindowReclaimDrainsRetiredScenesExactlyOnce);
  LOKA_RUN_TEST(testAppWindowCloseRequestsAreIdempotent);
  LOKA_RUN_TEST(testAppWindowClosedRejectsUndetachedOrActiveWindow);
  LOKA_RUN_TEST(testAppDrainsPendingWindowClosuresAtDestruction);
  LOKA_RUN_TEST(testSceneUnmountNotifiesPlainNodeContextDetached);
  LOKA_RUN_TEST(testBoundaryLocalRebuildNotifiesCompositionDetachedOnce);
  LOKA_RUN_TEST(testSceneUpdateAttachedFalseNotifiesPlainNodeContextDetachedOnce);
  LOKA_RUN_TEST(testSceneTeardownNotifiesBoundaryInternalNodeContextDetachedOnce);
  LOKA_RUN_TEST(testRootDetachChildWalkRetainsBoundaryStateOwner);
  LOKA_RUN_TEST(testRootUpdateFallbackDestroysRetiredArenaNodeOnNextTrackerRun);
  LOKA_RUN_TEST(testRootUpdateFallbackReservesFreshArenaGeneration);
  LOKA_RUN_TEST(testRetiredGenerationSubsumesQueuedArenaSubtreeExactlyOnce);
  LOKA_RUN_TEST(testRootUpdateFallbackReleasesNativeContextBeforeNodeOwnedStateReclaim);
  LOKA_RUN_TEST(testRetiredBoundaryIsQuiescentBeforeNextTrackerRun);
  LOKA_RUN_TEST(testRetiringNativeContextUnbindsBeforeNodeOwnedStateReclaim);
  LOKA_RUN_TEST(testSceneDestructionUnbindsNativeContextBeforeNodeOwnedStateReclaim);
  LOKA_RUN_TEST(testConditionalBranchSwapDestroysRetiredArenaNodeOnNextTrackerRun);
  LOKA_RUN_TEST(testRootReplacementDestroysRetiredArenaNodeOnNextTrackerRun);
  LOKA_RUN_TEST(testSceneTeardownDrainsNonEmptyRetiredArenaSubtreeExactlyOnce);
  LOKA_RUN_TEST(testSceneTeardownDrainsPendingRetiredGenerationExactlyOnce);
  LOKA_RUN_TEST(testConditionalBranchSwapDestroysRetiredArenaSubtreeChildrenFirst);
  LOKA_RUN_TEST(testRetiredArenaParentDestroysHeapChildExactlyOnceAtDrain);
  LOKA_RUN_TEST(testConditionalBranchSwapBackDestroysRetiredHeapNodeOnNextTrackerRun);
  LOKA_RUN_TEST(testPendingChildBoundaryUpdateSurvivesHeapSubtreeReplacement);
  LOKA_RUN_TEST(testRetiredHeapParentDestroysArenaChildExactlyOnceAtDrain);
  LOKA_RUN_TEST(testRetiredGenerationSubsumesQueuedHeapSubtreeExactlyOnce);
  LOKA_RUN_TEST(testRetiredSubtreeDestroysNestedBoundaryArenaExactlyOnce);
  LOKA_RUN_TEST(testRetiredBoundaryOwnedStateMutationIsQuiescent);
  LOKA_RUN_TEST(testSceneTeardownReleasesBothConditionalBranchContextsOnce);
  LOKA_RUN_TEST(testConditionalConditionWriteDuringDetachDoesNotMaterializeBranch);
  LOKA_RUN_TEST(testLokaValueCore);
  LOKA_RUN_TEST(testSnapFormatV1);
  LOKA_RUN_TEST(testSnapFlowWriteAdapter);
  LOKA_RUN_TEST(testLokaFlowDslV1Core);
  LOKA_RUN_TEST(testFlowChainHandleCopiesShareImplementationLifetime);
  LOKA_RUN_TEST(testFlowChainRunPinDefersImplementationDeletion);
  LOKA_RUN_TEST(testFlowSlotClearDefersRunningFlowDeletion);
  LOKA_RUN_TEST(testFlowSlotSetDefersRunningFlowDeletion);
  LOKA_RUN_TEST(testFlowSlotOwnerDestructionDefersRunningFlowDeletion);
  LOKA_RUN_TEST(testFlowOwnerDestructionReleasesPendingFlow);
  LOKA_RUN_TEST(testStateStreamCopyTransfersOwnedState);
  LOKA_RUN_TEST(testStateStreamAssignmentTransfersOwnedState);
  LOKA_RUN_TEST(testStateStreamDestructionReleasesOwnedState);
  LOKA_RUN_TEST(testStateStreamDestructionUnbindsSources);
  LOKA_RUN_TEST(testBoundaryBorrowDirectionsRejectSiblingAndDescendant);
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
  LOKA_RUN_TEST(testNodeArenaRetiredGenerationContracts);
  LOKA_RUN_TEST(SceneTests::runAll);
  LOKA_RUN_TEST(testPrepareProjectedLayoutDelegation);
  LOKA_RUN_TEST(testProjectedLayoutUsesActiveBoundaryModel);
  LOKA_RUN_TEST(testPrepareProjectedLayoutRejectsNullController);
  LOKA_RUN_TEST(testContainerLayoutHelpersAdvanceResultY);
  LOKA_RUN_TEST(testStartupRedrawCount_Before);
  LOKA_RUN_TEST(testStartupRedrawCount_After);
  LOKA_RUN_TEST(testToolboxChildDirtyInvalidationPrefersFullRedraw);
  LOKA_RUN_TEST(testToolboxManualInvalidateDoesNotSkipFollowupUpdateDraw);
  LOKA_RUN_TEST(testNodeDefaultsToDefaultNativeLifetimeHint);
  LOKA_RUN_TEST(testDefinitionCarriesNativeLifetimeHintToCreatedNode);
  LOKA_RUN_TEST(testDefinitionCloneAndApplyPreserveNativeLifetimeHint);
  LOKA_RUN_TEST(testDefinitionAssignmentCarriesNativeLifetimeHint);
  LOKA_RUN_TEST(testConditionalAndShowDefinitionsCarryNativeLifetimeHint);
  LOKA_RUN_TEST(testNativeContextObservesLifetimeHint);
  LOKA_RUN_TEST(testExactMatchBucketCountsHitsMissesEvictsAndDepth);
  LOKA_RUN_TEST(testExactMatchBucketDepthCapRefusesAndCountsEvicts);
  LOKA_RUN_TEST(testExactMatchBucketInstancesStayIsolatedAndReusableAfterDrain);
  LOKA_RUN_TEST(testNullPlatformContract_A1_contextDestructorRunsTeardownSequence);
  LOKA_RUN_TEST(testNullPlatformContract_A2_retainedDetachRunsNoTeardown);
  LOKA_RUN_TEST(testNullPlatformContract_A3_intakeConsistencyFailureLeaksWithoutPooling);
  LOKA_RUN_TEST(testNullPlatformContract_B1_attachShowsControl);
  LOKA_RUN_TEST(testNullPlatformContract_B2_retainedDetachHidesAndKeepsRow);
  LOKA_RUN_TEST(testNullPlatformContract_B3_reattachKeepsHandleIdentity);
  LOKA_RUN_TEST(testNullPlatformContract_B4_retireRemovesLedgerRow);
  LOKA_RUN_TEST(testNullPlatformContract_B5_hiddenAncestorSwapIsSilent);
  LOKA_RUN_TEST(testNullPlatformContract_C2_hintControlsFlushPolicy);
  LOKA_RUN_TEST(testNullPlatformContract_C3_hintChangesReachNextObservation);
  LOKA_RUN_TEST(testNullPlatformContract_D1_exactMatchBucketsStaySeparated);
  LOKA_RUN_TEST(testNullPlatformContract_D2_churnProducesPoolHits);
  LOKA_RUN_TEST(testNullPlatformContract_D3_depthCapRefusalCountsEvict);
  LOKA_RUN_TEST(testNullPlatformContract_D4_controllerDrainPrecedesWindowDispose);
  LOKA_RUN_TEST(testNullPlatformContract_E1_reclaimOnlyFlushIsSilent);
  LOKA_RUN_TEST(testNullPlatformContract_E2_disposeOccursOnlyAtSafePoints);
  LOKA_RUN_TEST(testNullPlatformContract_E3_parkedBranchRetiresAtTheDoorNotAtReclaim);
  LOKA_RUN_TEST(testNullPlatformContract_H1_conditionalSeatSurvivesUnrelatedRecompose);
  LOKA_RUN_TEST(testNullPlatformContract_H2_parkedDraftBranchSurvivesUnrelatedRecompose);
  LOKA_RUN_TEST(testConditionalSeatRepointsBranchDefinitionsAfterUnrelatedRecompose);
  LOKA_RUN_TEST(testNullPlatformContract_H3_conditionFlipIsReflectedAtNextScheduledApply);
  LOKA_RUN_TEST(testNullPlatformContract_H4_retiringBoundaryReportsEveryParkedBranchRetiredInSameTick);
  LOKA_RUN_TEST(testNullPlatformContract_H5_taggedSeatAmongSiblingsSurvivesUnrelatedRecompose);
  LOKA_RUN_TEST(testNullPlatformContract_H6_activeBranchContentIsFreshAfterRecompose);
  LOKA_RUN_TEST(testNestedConditionalSeatRepointsDefinitionsAtOuterReentry);
  LOKA_RUN_TEST(testShowDslParkedBranchIsCurrentAtReentry);
  LOKA_RUN_TEST(testStep4ShapeSettlesAfterShowFlip);
  LOKA_RUN_TEST(testStdCompositionBoundaryShowFlipPreservesSiblings);
  LOKA_RUN_TEST(testPolicyScopeIsDefinitionOnlyAndPreservesContentNativeHint);
  LOKA_RUN_TEST(testPolicyScopeRejectsNonBranchRootPlacement);
  LOKA_RUN_TEST(testPolicyScopeDestroyOnDetachContrastsWithDefaultInRecomposingBoundary);
  LOKA_RUN_TEST(testPolicyScopeDeliverWhileDetachedContrastsWithDefaultInRecomposingBoundary);
  LOKA_RUN_TEST(testPolicyScopeDestroyOnDetachWorksInComposeOnceBoundary);
  LOKA_RUN_TEST(testPolicyScopeDeliverWhileDetachedWorksInComposeOnceBoundary);
  LOKA_RUN_TEST(testComposeOnceBranchAtRootSeatSurvivesSnapshotRebuild);
  LOKA_RUN_TEST(testGenerationRetirementDoesNotLeaveStaleConditionalSeatMapping);
  LOKA_RUN_TEST(testRemovedConditionalSeatReaddsFreshRuntimeAndBranches);
  LOKA_RUN_TEST(testConditionalSeatInitiallyNullCanMaterialize);
  LOKA_RUN_TEST(testNullConditionalBranchParksAndReentersShownBranch);
  LOKA_RUN_TEST(testDepth2NestedConditionalSeatRepointsDefinitionsAtOuterReentry);
  LOKA_RUN_TEST(testFullRebuildSubsumesParkedBranchLedgerGeneration);
  LOKA_RUN_TEST(testIncompatibleParkedBranchRootsRetireAndRecreateAtReentry);
  LOKA_RUN_TEST(testNullPlatformContract_H7_reenteredBranchContentIsFreshAfterRecompose);
  LOKA_RUN_TEST(testNullPlatformContract_H8_taggedSeatBuildsBranchFromLiveDefinition);
  LOKA_RUN_TEST(testNullPlatformContract_H9_retainedSeatUsesReplacementCondition);
  LOKA_RUN_TEST(testNullPlatformContract_F1_retiredQueueIsEmptyAfterFlush);
  LOKA_RUN_TEST(testNullPlatformContract_F2_createdHandlesAreDisposedAtTeardown);
  LOKA_RUN_TEST(testNullPlatformContract_G4_retireBeforeContextAttachIsSilent);
  LOKA_RUN_TEST(testNullWindowScenePathMountsAndTearsDownBeforeControllerDelete);
  LOKA_RUN_TEST(testNullPlatformContract_G6_materializedChildIsVisibleInSameRun);
  LOKA_RUN_TEST(testLifecycleFactBornAttachedAndSwapWritesRetainedDetach);
  LOKA_RUN_TEST(testLifecycleFactTerminalDetachObservesRetired);
  LOKA_RUN_TEST(testLifecycleFactCompositionRetireObservesRetired);
  LOKA_RUN_TEST(testLifecycleFactWalkIsSilentAndDeliveryIsDiffBased);
  LOKA_RUN_TEST(testLifecycleFactChildAdoptedUnderHiddenAncestorInheritsDetached);
  LOKA_AUDIT_CHECKPOINT("FlowDslTestMain final");
  return 0;
}

#undef LOKA_RUN_TEST
