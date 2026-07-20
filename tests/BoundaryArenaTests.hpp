#ifndef LOKA_BOUNDARY_ARENA_TESTS_HPP
#define LOKA_BOUNDARY_ARENA_TESTS_HPP

void testBoundaryArenaContracts();
void testNodeArenaRetiredGenerationContracts();
void testStateArenaSlabsCrossAllocationGate();
void testNodeArenaSlabCrossesAllocationGate();
void testHeapNodeCrossesAllocationGate();
void testComposeAllocationWhiteFlagDefersFullRebuildToNextExternalTick();
void testHeapFallbackWhiteFlagFailsBoundaryCompose();
void testSceneRootAllocationRefusalArmsWhiteFlagAndHealsOnRefresh();

#endif // LOKA_BOUNDARY_ARENA_TESTS_HPP
