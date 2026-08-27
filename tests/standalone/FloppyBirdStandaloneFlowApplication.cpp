#include "FloppyBirdStandaloneFlowApplication.hpp"

#include "FloppyBirdStandaloneFlowAppConfig.hpp"
#include "StandaloneFlowRunner.hpp"

namespace loka
{
  namespace standalone_tests
  {
    int RunFloppyBirdStandaloneFlowApplication(HINSTANCE hInstance, int nCmdShow)
    {
      return RunStandaloneFlowWithConfig<FloppyBirdStandaloneFlowAppConfig>(hInstance, nCmdShow);
    }
  } // namespace standalone_tests
} // namespace loka
