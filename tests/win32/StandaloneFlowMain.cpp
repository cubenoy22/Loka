#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "StandaloneFlowApplication.hpp"

#if !defined(TEST_BUILD)
#error LokaScrapbookStandaloneFlowWin32 requires TEST_BUILD
#endif

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR commandLine, int nCmdShow)
{
  (void)hPrevInstance;
  (void)commandLine;
  return loka::standalone_tests::RunStandaloneFlowApplication(hInstance, nCmdShow);
}
