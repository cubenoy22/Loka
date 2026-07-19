#ifndef LOKA_FLOW_DSL_TESTS_HPP
#define LOKA_FLOW_DSL_TESTS_HPP

void testLokaFlowDslV1Core();
void testSimpleViewerClosesDialogFromChooserCompletion();
void testFlowChainHandleCopiesShareImplementationLifetime();
void testFlowChainRunPinDefersImplementationDeletion();
void testFlowSlotClearDefersRunningFlowDeletion();
void testFlowSlotSetDefersRunningFlowDeletion();
void testFlowSlotOwnerDestructionDefersRunningFlowDeletion();
void testFlowOwnerDestructionReleasesPendingFlow();
void testStateStreamCopyTransfersOwnedState();
void testStateStreamAssignmentTransfersOwnedState();
void testStateStreamDestructionReleasesOwnedState();
void testStateStreamDestructionUnbindsSources();
void testBoundaryBorrowDirectionsRejectSiblingAndDescendant();

#endif // LOKA_FLOW_DSL_TESTS_HPP
