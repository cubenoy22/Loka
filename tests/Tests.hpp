#ifndef LOKA_TESTS_HPP
#define LOKA_TESTS_HPP

void testDependencyPropagationCases();
void testTrackerPropagation();
void testDeferredSideEffect();
void testTextInputOnChange();
void testBatchTransaction();
void testRAIITransaction();
void testDerivedStruct();
void testSceneManagerTransaction();
void testNodeCompositionTree();
void testNodeCompositionShowIf();
void testSceneMountLifecycle();
void testSceneBoundaryNestedCompose();
void testStaticBoundaryPropagatesUpdateToDynamicChild();
void testDynamicBoundaryRecomposesOnlyOnChildDirty();
void testBoundaryDirtyPolicyStaticImmediateDynamicDeferred();
void testSceneInvalidateUsesRequestedDirtyFlags();
void testSceneRequestInvalidateDefersUntilFlush();
void testLokaCoreString();
void testLokaCoreCollections();
void testLokaDslStream();
void testStateBatchOverflow();
void testNestedTransaction();
void testNestedTransactionInvalidateTiming();

#endif // LOKA_TESTS_HPP
