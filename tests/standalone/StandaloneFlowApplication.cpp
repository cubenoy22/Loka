#include "StandaloneFlowApplication.hpp"

#include "ScrapbookStandaloneFlowAppConfig.hpp"
#include "StandaloneFlowRunner.hpp"

namespace loka
{
  namespace standalone_tests
  {
    int RunStandaloneFlowApplication(NativeModuleHandle hInstance, int nCmdShow)
    {
      return RunStandaloneFlowWithConfig<ScrapbookStandaloneFlowAppConfig>(hInstance, nCmdShow);
    }
  } // namespace standalone_tests
} // namespace loka
