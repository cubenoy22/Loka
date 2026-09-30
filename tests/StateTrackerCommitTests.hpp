#ifndef LOKA_STATE_TRACKER_COMMIT_TESTS_HPP
#define LOKA_STATE_TRACKER_COMMIT_TESTS_HPP

void testStateTrackerCommitQueuesNextTransaction();
void testStateTrackerCommitWriteReachesNextSceneApply();
void testStateTrackerCommitChainReportsIterationLimit();
void testStateTrackerGuardOpenedDuringSettlementJoinsTransaction();
void testStateTrackerGuardOpenedDuringCommitJoinsTransaction();

void testB1GenerationFreshBegin();
void testB1GenerationNestedBegin();
void testB1GenerationReentrantBegin();
void testB1GenerationNextIntake();
void testB1GenerationLimitExit();
void testB1GenerationLimitBeginNoDoubleAdvance();
void testB1GenerationTerminalBegin();
void testB1GenerationTerminalAdvance();

void testB1GenerationTerminalStaticInitialization();

#endif // LOKA_STATE_TRACKER_COMMIT_TESTS_HPP
