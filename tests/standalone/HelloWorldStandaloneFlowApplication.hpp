#ifndef LOKA_TESTS_HELLO_WORLD_STANDALONE_FLOW_APPLICATION_HPP
#define LOKA_TESTS_HELLO_WORLD_STANDALONE_FLOW_APPLICATION_HPP

#include "app/PlatformContext.hpp"

namespace loka
{
  namespace standalone_tests
  {
    /** Runs HelloWorld's typed toggle/action probe until the user quits. */
    int RunHelloWorldStandaloneFlowApplication(HINSTANCE hInstance = 0, int nCmdShow = 0);
  } // namespace standalone_tests
} // namespace loka

#endif // LOKA_TESTS_HELLO_WORLD_STANDALONE_FLOW_APPLICATION_HPP
