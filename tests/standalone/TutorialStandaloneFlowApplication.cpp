#include "TutorialStandaloneFlowApplication.hpp"

#include "StandaloneFlowRunner.hpp"
#include "TutorialStandaloneFlowAppConfig.hpp"

namespace loka
{
  namespace standalone_tests
  {
    int RunTutorialStandaloneFlowApplication(HINSTANCE hInstance, int nCmdShow)
    {
      return RunStandaloneFlowWithConfig<TutorialStandaloneFlowAppConfig>(hInstance, nCmdShow);
    }
  } // namespace standalone_tests
} // namespace loka
