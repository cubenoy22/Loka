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
  testStaticNodeManagerRun();
  // SceneTests::runAll();
  return 0;
}
