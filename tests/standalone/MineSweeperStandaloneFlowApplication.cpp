#include "MineSweeperStandaloneFlowApplication.hpp"

#include "MineSweeperStandaloneFlowAppConfig.hpp"
#include "StandaloneFlowRunner.hpp"

namespace loka
{
  namespace standalone_tests
  {
    int RunMineSweeperStandaloneFlowApplication(HINSTANCE hInstance, int nCmdShow)
    {
      return RunStandaloneFlowWithConfig<MineSweeperStandaloneFlowAppConfig>(hInstance, nCmdShow);
    }
  } // namespace standalone_tests
} // namespace loka
