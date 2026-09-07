#include "SimpleViewerScenarioDriver.hpp"

#if !defined(LOKA_RETRO68)
#error LokaSimpleViewerTestsToolbox is a Retro68-only application
#endif

#if !defined(TEST_BUILD)
#error LokaSimpleViewerTestsToolbox requires TEST_BUILD
#endif

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  return loka::toolbox_tests::RunSimpleViewerScenarioApplication();
}
