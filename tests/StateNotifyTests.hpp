#ifndef LOKA_STATE_NOTIFY_TESTS_HPP
#define LOKA_STATE_NOTIFY_TESTS_HPP

void testStateNotify();
void testStateAssignmentBindSelfDeletion();
void testStateForcedAssignmentBindSelfDeletion();
void testStateDeferredNotifySelfDeletion();
void testStateLifetimeTokenIdentity();
void testStateExternalGuardSurvivesDestructionAndAddressReuse();
void testDialogExternalGuardProtectsUnobservedEmitter();


void testNextTickRunScopeRefusesRun();
void testNextTickNestedRunScopeIsInert();
void testNextTickRunScopePublishesRequestOnClose();

#endif // LOKA_STATE_NOTIFY_TESTS_HPP
