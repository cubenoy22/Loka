#include "StandaloneFlowAppConfig.hpp"
#include "StandaloneFlowRunner.hpp"

#if !defined(LOKA_RETRO68) || !defined(TEST_BUILD)
#error SmirkyCard standalone Flow requires a Classic test build
#endif

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  return loka::standalone_tests::RunStandaloneFlowWithConfig<SmirkyCardStandaloneFlowAppConfig>(0, 0);
}
