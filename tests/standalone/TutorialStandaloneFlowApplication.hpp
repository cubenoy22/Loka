#ifndef LOKA_TESTS_TUTORIAL_STANDALONE_FLOW_APPLICATION_HPP
#define LOKA_TESTS_TUTORIAL_STANDALONE_FLOW_APPLICATION_HPP

#include "app/PlatformContext.hpp"

namespace loka
{
  namespace standalone_tests
  {
    /** Runs Tutorial's increment/summary scenario until the user quits. */
    int RunTutorialStandaloneFlowApplication(HINSTANCE hInstance = 0, int nCmdShow = 0);
  } // namespace standalone_tests
} // namespace loka

#endif // LOKA_TESTS_TUTORIAL_STANDALONE_FLOW_APPLICATION_HPP
