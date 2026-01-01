#include "Tests.hpp"

int main()
{
  testDependencyPropagationCases();
  testTrackerPropagation();
  testDeferredSideEffect();
  testBatchTransaction();
  testRAIITransaction();
  testTextInputOnChange();
  testDerivedStruct();
  testSceneManagerTransaction();
  testComponentContext();
  testNodeCompositionTree();
  testSceneMountLifecycle();
  testLokaCoreString();
  testLokaCoreCollections();
  // SceneTests::runAll();
  return 0;
}
