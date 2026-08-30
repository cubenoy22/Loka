#ifndef LOKA_NODE_MATCH_TESTS_HPP
#define LOKA_NODE_MATCH_TESTS_HPP

void testNodeMatchSelectsFirstDeclaredValueArm();
void testNodeMatchPredicatePrecedesValueArmAndRunsOncePerVisit();
void testNodeMatchEmptySeatRematerializes();
void testNodeMatchOtherwiseIsTheLastArm();
void testNodeMatchRestoresThreeIndependentArmStates();
void testNodeMatchDestroyOnDetachIsPerArm();
void testNodeMatchCapacityRefusesOverflow();

#endif // LOKA_NODE_MATCH_TESTS_HPP
