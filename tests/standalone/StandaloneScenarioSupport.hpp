#ifndef LOKA_TESTS_STANDALONE_SCENARIO_SUPPORT_HPP
#define LOKA_TESTS_STANDALONE_SCENARIO_SUPPORT_HPP

#include <cstdio>

#include "../scenarios/ScenarioTypes.hpp"
#include "platform/file/FileHandle.hpp"

class App;
class Window;

namespace loka
{
  namespace standalone_tests
  {
    /** Reports a standalone Window's content-local bounds when available. */
    scenario_tests::CaptureContentBounds StandaloneContentBounds(Window *window);

    /** Resolves the application-side audit file shared by presentation apps. */
    platform::file::FileHandle ResolveStandaloneAuditFile();

    /** Owns the bounded startup wait shared by the TEST-only standalone
        presentations. A refused MainNode mount produces one diagnostic,
        requests App shutdown, and remains a terminal failure. */
    class StandaloneMountDeadline
    {
    public:
      enum Advance
      {
        ADVANCE_WAITING,
        ADVANCE_MOUNTED,
        ADVANCE_FAILED
      };

      explicit StandaloneMountDeadline(const char *applicationName, std::FILE *diagnostics = 0);

      void setApp(App *app);
      Advance advance(bool mainNodeMounted);
      long tick() const;
      bool failed() const;

    private:
      enum
      {
        MOUNT_DEADLINE_TICKS = 5
      };

      App *borrowedApp_;
      const char *applicationName_;
      std::FILE *diagnostics_;
      long tick_;
      bool failed_;
    };
  } // namespace standalone_tests
} // namespace loka

#endif // LOKA_TESTS_STANDALONE_SCENARIO_SUPPORT_HPP
