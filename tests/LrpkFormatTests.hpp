#ifndef LOKA_LRPK_FORMAT_TESTS_HPP
#define LOKA_LRPK_FORMAT_TESTS_HPP

void testLrpkRoundTripsThroughTheIndex();
void testLrpkSelectsRepresentationByAxisKind();
void testLrpkRefusesEveryCheckValueFailure();
void testLrpkUnsafeModeOmitsRotButNotIdentity();
void testLrpcRefusesPackagesThatWouldMakeSelectionPartial();
void testLrpcRefusesRowsThatWouldNotBeReachable();
void testLrpkRefusesIndexGeometryThatWouldReadOutOfBounds();

#endif // LOKA_LRPK_FORMAT_TESTS_HPP
