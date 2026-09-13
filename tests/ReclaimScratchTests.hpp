#ifndef LOKA_RECLAIM_SCRATCH_TESTS_HPP
#define LOKA_RECLAIM_SCRATCH_TESTS_HPP
void testReclaimScratchBoundaryWide();
void testReclaimScratchBoundaryDeep();
void testReclaimScratchGenerationWide();
void testReclaimScratchGenerationDeep();
void testReclaimScratchPartitionWide();
void testReclaimScratchPartitionDeep();
void testReclaimScratchOverflow();
void testReclaimScratchNestedBoundary();
void testReclaimScratchPartitionUnattached();
void testReclaimScratchPartitionBoundaryChild();
void testReclaimScratchPartitionBoundaryKeepsOwnChildren();
void testReclaimScratchBoundaryOverflowFallback();
void testReclaimScratchGenerationOverflowFallback();
#endif
