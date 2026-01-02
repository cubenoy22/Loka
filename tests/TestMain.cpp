#include "SceneTests.hpp"
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
  testNodeCompositionTree();
  testSceneMountLifecycle();
  testSceneBoundaryNestedCompose();
  testLokaCoreString();
  testLokaCoreCollections();
  SceneTests::runAll();
  return 0;
}
