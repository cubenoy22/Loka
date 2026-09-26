#ifndef LOKA_STATE_STREAM_CHAIN_TESTS_HPP
#define LOKA_STATE_STREAM_CHAIN_TESTS_HPP

void testStateStreamChainMapOwnsLinks();
void testStateStreamChainCombineLeftOwnsLinks();
void testStateStreamChainCombineRightOwnsLinks();
void testStateStreamChainReturnedOwnsLinks();
void testStateStreamChainExprOwnsLinks();
void testStateStreamChainNamedConsumptionEscapes();
void testStateStreamChainFlowSlotOwnsLinks();
void testStateStreamChainCopyAssignmentAndRepeatedRelease();
void testStateStreamChainReleasesIntermediatesInReverseOrder();

#endif
