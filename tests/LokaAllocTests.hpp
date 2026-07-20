#ifndef LOKA_TESTS_LOKA_ALLOC_TESTS_HPP
#define LOKA_TESTS_LOKA_ALLOC_TESTS_HPP

void testLokaAllocDefaultBackendRoundTrip();
void testLokaNewReturnsNullWhenBackendRefusesNthAllocation();
void testLokaAllocBackendResetRestoresDefault();
void testLokaAllocAuditBalancedUseCountsToZero();

#endif // LOKA_TESTS_LOKA_ALLOC_TESTS_HPP
