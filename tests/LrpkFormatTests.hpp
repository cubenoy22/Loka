#ifndef LOKA_LRPK_FORMAT_TESTS_HPP
#define LOKA_LRPK_FORMAT_TESTS_HPP

void testLrpkRoundTripsThroughTheIndex();
void testLrpkSelectsByPackagePrecedence();
void testLrpkRefusesEveryCheckValueFailure();
void testLrpkOpenControlsIntegrityVerification();
void testLrpcRefusesPackagesThatWouldMakeSelectionPartial();
void testLrpcRefusesRowsThatWouldNotBeReachable();
void testLrpkRefusesIndexGeometryThatWouldReadOutOfBounds();
void testLrpkRefusesForgedCountsAndUnsortedRows();
void testLrpcValidatesBeforeItPacks();
void testLrpkReaderKeepsItsPackageWhenAReloadIsRefused();
void testLrpkChecksTheChunkThatDecidesSelection();
void testLrpkEnforcesPayloadAlignment();
void testLrpcPreservesNullPayloadFailure();

#endif // LOKA_LRPK_FORMAT_TESTS_HPP
