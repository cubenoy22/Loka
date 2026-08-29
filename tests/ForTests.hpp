#ifndef LOKA_FOR_TESTS_HPP
#define LOKA_FOR_TESTS_HPP

void testForIndexBuildsHandWrittenSectionTree();
void testForRejectsDuplicateKeysWithinBatch();
void testForRejectsInvalidTagsBeforeInsertion();
void testForFactoryCloneFailureLeavesParentUnchanged();
void testUniqueTaggedSiblingListRejectsAnonymousSibling();
void testForDerivedKeysRetainItemSeatAcrossRemoval();
void testForVectorBuilderReadsCurrentContentsAtAppend();

#endif // LOKA_FOR_TESTS_HPP
