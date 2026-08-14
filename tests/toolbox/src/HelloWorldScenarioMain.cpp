#include "HelloWorldScenarioDriver.hpp"

#if !defined(LOKA_RETRO68)
#error LokaHelloWorldTestsToolbox is a Retro68-only application
#endif

#if !defined(TEST_BUILD)
#error LokaHelloWorldTestsToolbox requires TEST_BUILD
#endif

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  return loka::toolbox_tests::RunHelloWorldScenarioApplication();
}
