#include "SimpleTextScenarioDriver.hpp"

#if !defined(LOKA_RETRO68)
#error LokaSimpleTextTestsToolbox is a Retro68-only application
#endif

#if !defined(TEST_BUILD)
#error LokaSimpleTextTestsToolbox requires TEST_BUILD
#endif

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  return loka::toolbox_tests::RunSimpleTextScenarioApplication();
}
