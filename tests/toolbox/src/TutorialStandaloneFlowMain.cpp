#include "TutorialStandaloneFlowApplication.hpp"

#if !defined(LOKA_RETRO68)
#error LokaTutorialStandaloneFlow is a Retro68-only application
#endif

#if !defined(TEST_BUILD)
#error LokaTutorialStandaloneFlow requires TEST_BUILD
#endif

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  return loka::standalone_tests::RunTutorialStandaloneFlowApplication();
}
