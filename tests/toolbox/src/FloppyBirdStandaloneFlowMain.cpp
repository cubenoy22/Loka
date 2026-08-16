#include "FloppyBirdStandaloneFlowApplication.hpp"

#if !defined(LOKA_RETRO68)
#error LokaFloppyStandaloneFlow is a Retro68-only application
#endif

#if !defined(TEST_BUILD)
#error LokaFloppyStandaloneFlow requires TEST_BUILD
#endif

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  return loka::standalone_tests::RunFloppyBirdStandaloneFlowApplication();
}
