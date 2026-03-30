#include "StateNotifyTests.hpp"
#include "FlowDslTests.hpp"
#include "AttrDslTests.hpp"
#include "SnapFormatTests.hpp"
#include "StartupRedrawTests.hpp"

int main() {
  testStateNotify();
  testSnapFormatV1();
  testSnapFlowWriteAdapter();
  testLokaFlowDslV1Core();
  testLokaAttrDslV1Core();
  testPlatformNodeHandlerRegistration();
  testPlatformNodeHandlerReplacement();
  testPlatformNodeHandlerRejectsInvalidTypeKey();
  testPlatformLayoutHandlerRegistration();
  testPlatformLayoutTraversalResultY();
  testPlatformLayoutHandlerReplacement();
  testPlatformLayoutHandlerRejectsInvalidTypeKey();
  testPlatformLayoutHandlerSamePointerReregister();
  testPrepareProjectedLayoutDelegation();
  testProjectedLayoutUsesActiveBoundaryModel();
  testPrepareProjectedLayoutRejectsNullController();
  testContainerLayoutHelpersAdvanceResultY();
  testStartupRedrawCount_Before();
  testStartupRedrawCount_After();
  testToolboxChildDirtyInvalidationPrefersFullRedraw();
  return 0;
}
