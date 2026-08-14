#ifndef LOKA_TESTS_STANDALONE_FLOW_APPLICATION_HPP
#define LOKA_TESTS_STANDALONE_FLOW_APPLICATION_HPP

#include "app/PlatformContext.hpp"

namespace loka
{
  namespace standalone_tests
  {
    /** Runs the compiled Scrapbook presentation until the user quits. */
    int RunStandaloneFlowApplication(HINSTANCE hInstance = 0, int nCmdShow = 0);
  } // namespace standalone_tests
} // namespace loka

#endif
