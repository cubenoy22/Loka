#ifndef LOKA_TESTS_MINESWEEPER_STANDALONE_FLOW_APPLICATION_HPP
#define LOKA_TESTS_MINESWEEPER_STANDALONE_FLOW_APPLICATION_HPP

#include "app/PlatformContext.hpp"

namespace loka
{
  namespace standalone_tests
  {
    /** Runs MineSweeper's fixed-seed New Game tour until the user quits. */
    int RunMineSweeperStandaloneFlowApplication(HINSTANCE hInstance = 0, int nCmdShow = 0);
  } // namespace standalone_tests
} // namespace loka

#endif // LOKA_TESTS_MINESWEEPER_STANDALONE_FLOW_APPLICATION_HPP
