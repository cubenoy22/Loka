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
void testLrpcRoundTripsExactlySizedSelectors();
void testLrpcRefusesAxisDeclarationAfterAsset();
void testLrpkReaderKeepsItsPackageWhenAReloadIsRefused();
void testLrpkChecksTheChunkThatDecidesSelection();
void testLrpkRequiresCanonicalChunkOrder();
void testLrpkEnforcesPayloadAlignment();
void testLrpcPreservesNullPayloadFailure();
void testLrpcCanonicalBuildBytesStayStable();
void testLrpcBuildHandlesFiftyThousandAssets();
void testLrpkStreamOpenMatchesMemoryOpen();
void testLrpkStreamOpenIsFailureAtomic();
void testLrpkStreamRefusesSourceLies();
void testLrpkReadBagIntoWalksTheSameRefusalOrder();
void testLrpkStreamOpensEmptyAndZeroLengthBags();
void testBlobSealBytesFreezesSizeAndCompletion();

#endif // LOKA_LRPK_FORMAT_TESTS_HPP
