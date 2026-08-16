#ifndef LOKA_TESTS_FLOPPY_BIRD_STANDALONE_FLOW_APPLICATION_HPP
#define LOKA_TESTS_FLOPPY_BIRD_STANDALONE_FLOW_APPLICATION_HPP

#include "app/PlatformContext.hpp"

namespace loka
{
  namespace standalone_tests
  {
    /** Runs FloppyBird's fixed-step flap tour until the user quits. */
    int RunFloppyBirdStandaloneFlowApplication(HINSTANCE hInstance = 0, int nCmdShow = 0);
  } // namespace standalone_tests
} // namespace loka

#endif // LOKA_TESTS_FLOPPY_BIRD_STANDALONE_FLOW_APPLICATION_HPP
