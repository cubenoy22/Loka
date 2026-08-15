#ifndef LOKA_TESTS_STANDALONE_SCENARIO_SUPPORT_HPP
#define LOKA_TESTS_STANDALONE_SCENARIO_SUPPORT_HPP

#include "../scenarios/ScenarioTypes.hpp"
#include "platform/file/FileHandle.hpp"

class Window;

namespace loka
{
  namespace standalone_tests
  {
    /** Reports a standalone Window's content-local bounds when available. */
    scenario_tests::CaptureContentBounds StandaloneContentBounds(Window *window);

    /** Resolves the application-side audit file shared by presentation apps. */
    platform::file::FileHandle ResolveStandaloneAuditFile();
  } // namespace standalone_tests
} // namespace loka

#endif // LOKA_TESTS_STANDALONE_SCENARIO_SUPPORT_HPP
