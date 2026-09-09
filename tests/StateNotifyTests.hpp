#ifndef LOKA_STATE_NOTIFY_TESTS_HPP
#define LOKA_STATE_NOTIFY_TESTS_HPP

void testStateNotify();
void testStateDeferredNotifySelfDeletion();
void testStateLifetimeTokenIdentity();
void testStateExternalGuardSurvivesDestructionAndAddressReuse();
void testDialogExternalGuardProtectsUnobservedEmitter();


#endif // LOKA_STATE_NOTIFY_TESTS_HPP
