#include "HelloWorldScenarioLoop.hpp"

#if !defined(LOKA_RETRO68)
#error LokaHelloScenarioLoop is a Retro68-only application
#endif

#if !defined(TEST_BUILD)
#error LokaHelloScenarioLoop requires TEST_BUILD
#endif

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  return loka::toolbox_tests::RunHelloWorldScenarioLoopApplication();
}
