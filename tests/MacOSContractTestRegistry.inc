// macOS-only contract tests. Kept nested so non-Apple CMake enumeration does
// not register tests whose Objective-C++ translation unit is absent.
LOKA_TEST(testMacDisplayAppearanceDeclinesWithoutMatchingCapability)
LOKA_TEST(testMacDisplayAppearanceUsesNativeBestMatch)
LOKA_TEST(testMacApplicationItemNamesExecutableDirectory)
