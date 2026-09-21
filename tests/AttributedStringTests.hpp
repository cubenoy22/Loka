#ifndef LOKA_TESTS_ATTRIBUTED_STRING_TESTS_HPP
#define LOKA_TESTS_ATTRIBUTED_STRING_TESTS_HPP

void testAttributedStringBuilderAllocations();
void testAttributedStringBuilderGrowth();
void testAttributedStringBuilderRefusal();
void testAttributedStringBuilderLifetime();
void testAttributedStringBuilderEquality();

void testAttributedStringContentEquality();
void testAttributedStringOrdering();
void testAttributedStringStyleDirections();
void testAttributedStringEmptyAndInvalid();
void testAttributedStringStateContentEquality();
void testAttributedStringAllocationFailures();
void testAttributedStringRefusedBuffersAndSharedFastPath();
void testManagedTryWrapFailureAndRelease();

#endif
