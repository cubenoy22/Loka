#include "MineSweeperStandaloneFlowApplication.hpp"

#if !defined(LOKA_RETRO68)
#error LokaMineStandaloneFlow is a Retro68-only application
#endif

#if !defined(TEST_BUILD)
#error LokaMineStandaloneFlow requires TEST_BUILD
#endif

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  return loka::standalone_tests::RunMineSweeperStandaloneFlowApplication();
}
