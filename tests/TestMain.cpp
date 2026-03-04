#include "SceneTests.hpp"
#include "FlowDslTests.hpp"
#include "Tests.hpp"

int main()
{
  testDependencyPropagationCases();
  testTrackerPropagation();
  testDeferredSideEffect();
  testBatchTransaction();
  testRAIITransaction();
  testNestedTransaction();
  testNestedTransactionInvalidateTiming();
  testTextInputOnChange();
  testDerivedStruct();
  testSceneManagerTransaction();
  testNodeCompositionTree();
  testSceneMountLifecycle();
  testSceneBoundaryNestedCompose();
  testLokaCoreString();
  testLokaCoreCollections();
  testLokaDslStream();
  testLokaFlowDslV1Core();
  testStateBatchOverflow();
  SceneTests::runAll();
  return 0;
}
