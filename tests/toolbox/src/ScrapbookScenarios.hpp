#ifndef LOKA_TESTS_TOOLBOX_SCRAPBOOK_SCENARIOS_HPP
#define LOKA_TESTS_TOOLBOX_SCRAPBOOK_SCENARIOS_HPP

#include <string>

#include "testing/snap/SnapFormat.hpp"

namespace scrapbook
{
  class MainNode;
}

namespace loka
{
  namespace toolbox_tests
  {
    struct ContentBounds
    {
      ContentBounds()
          : available(false),
            left(0),
            top(0),
            right(0),
            bottom(0)
      {
      }

      bool available;
      long left;
      long top;
      long right;
      long bottom;
    };

    bool IsRegisteredScenario(const std::string &name);
    dsl::SnapRecord
    RunRegisteredScenario(const std::string &name, const scrapbook::MainNode &mainNode, const ContentBounds &bounds);
    dsl::SnapRecord MakeDriverErrorRecord(const char *scenario, long errorCode, const char *message);
  } // namespace toolbox_tests
} // namespace loka

#endif // LOKA_TESTS_TOOLBOX_SCRAPBOOK_SCENARIOS_HPP
