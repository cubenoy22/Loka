#include "ScenarioDriver.hpp"

#if !defined(LOKA_RETRO68)
#error LokaTestsToolbox is a Retro68-only application
#endif

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  return loka::toolbox_tests::RunScenarioApplication();
}
