#ifndef LOKA_PARTITION_RECLAIM_TESTS_HPP
#define LOKA_PARTITION_RECLAIM_TESTS_HPP
void testPartitionReclaimClockAndExactlyOnce();
void testPartitionReclaimRetainsBackingTwentyRounds();
void testPartitionReclaimParkedArm();
void testPartitionReclaimNestedAndFailedCandidate();
void testPartitionReclaimLandlordCensus();
#endif
