#include "StateNotifyTests.hpp"
#include "FlowDslTests.hpp"
#include "AttrDslTests.hpp"

int main() {
  testStateNotify();
  testLokaFlowDslV1Core();
  testLokaAttrDslV1Core();
  return 0;
}
