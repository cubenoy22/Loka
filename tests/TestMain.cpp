#include "SceneTests.hpp"
#include "StateNotifyTests.hpp"
#include "FlowDslTests.hpp"
#include "AttrDslTests.hpp"
#include "Tests.hpp"

int main()
{
  testStateNotify();
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
  testNodeCompositionShowIf();
  testSceneMountLifecycle();
  testSceneBoundaryNestedCompose();
  testStaticBoundaryPropagatesUpdateToDynamicChild();
  testLokaCoreString();
  testLokaCoreCollections();
  testLokaDslStream();
  testLokaFlowDslV1Core();
  testLokaAttrDslV1Core();
  testStateBatchOverflow();
  SceneTests::runAll();
  return 0;
}
