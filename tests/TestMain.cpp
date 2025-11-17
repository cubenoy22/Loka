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
  testStaticNodeManagerRun();
  // SceneTests::runAll();
  return 0;
}
