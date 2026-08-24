#define WIN32_LEAN_AND_MEAN

#if !defined(LOKA_STANDALONE_FLOW_CONFIG_HEADER)
#error StandaloneFlowMain requires LOKA_STANDALONE_FLOW_CONFIG_HEADER
#endif

#if !defined(LOKA_STANDALONE_FLOW_CONFIG_TYPE)
#error StandaloneFlowMain requires LOKA_STANDALONE_FLOW_CONFIG_TYPE
#endif

#include LOKA_STANDALONE_FLOW_CONFIG_HEADER
#include "StandaloneFlowRunner.hpp"

#if !defined(TEST_BUILD)
#error Loka Standalone Flow Win32 requires TEST_BUILD
#endif

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR commandLine, int nCmdShow)
{
  (void)hPrevInstance;
  (void)commandLine;
  return loka::standalone_tests::RunStandaloneFlowWithConfig<LOKA_STANDALONE_FLOW_CONFIG_TYPE>(hInstance,
                                                                                              nCmdShow);
}
