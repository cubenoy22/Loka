#ifndef LOKA_TESTS_LAZY_SCOPE_TESTS_HPP
#define LOKA_TESTS_LAZY_SCOPE_TESTS_HPP
void testLazyScopeMaterializesOwnedDeclaration();
void testLazyScopeOwnTrackerAppliesShow();
void testLazyScopeCrossesTrackersInOneUpdate();
void testLazyScopeKeyReplacementRetiresOwner();
void testLazyScopeStateRefusalPreservesArm();
void testLazyScopeUnmountCancelsWatch();
void testLazyScopeOuterDestroyRecreatesOwner();
#endif
