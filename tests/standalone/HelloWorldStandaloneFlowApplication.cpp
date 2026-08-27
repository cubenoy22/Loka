#include "HelloWorldStandaloneFlowApplication.hpp"

#include "HelloWorldStandaloneFlowAppConfig.hpp"
#include "StandaloneFlowRunner.hpp"

namespace loka
{
  namespace standalone_tests
  {
    int RunHelloWorldStandaloneFlowApplication(HINSTANCE hInstance, int nCmdShow)
    {
      return RunStandaloneFlowWithConfig<HelloWorldStandaloneFlowAppConfig>(hInstance, nCmdShow);
    }
  } // namespace standalone_tests
} // namespace loka
