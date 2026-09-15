#include "SmirkyCardScenarioDriver.hpp"
#if !defined(LOKA_RETRO68) || !defined(TEST_BUILD)
#error LokaSmirkyCardTestsToolbox requires Retro68 TEST_BUILD
#endif
int main(int, char **) { return loka::toolbox_tests::RunSmirkyCardScenarioApplication(); }
