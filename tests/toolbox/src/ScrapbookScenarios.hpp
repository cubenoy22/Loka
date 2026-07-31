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

    /** Owns one step-driven scenario's observations between idle ticks. */
    class ScrapbookScenario
    {
    public:
      explicit ScrapbookScenario(const std::string &name);

      /** Drives one scenario step. Returns true only when out contains the
          final record and the driver may begin its post-record linger. */
      bool step(long tick, scrapbook::MainNode &mainNode, const ContentBounds &bounds, dsl::SnapRecord &out);

    private:
      enum Kind
      {
        KIND_INVALID = 0,
        KIND_OPEN_FIRST_PAGE,
        KIND_OPEN_FIRST_PAGE_REFUSED,
        KIND_FLIP_FORWARD_BACK,
        KIND_REFUSED_FLIP_KEEPS_PAGE,
        KIND_OPEN_TEXT_PAGE
      };

      struct PageObservation
      {
        PageObservation()
            : published(false),
              page(-1),
              captionAvailable(false),
              caption()
        {
        }

        bool published;
        int page;
        bool captionAvailable;
        std::string caption;
      };

      bool runOpenScenario(long tick,
                           const scrapbook::MainNode &mainNode,
                           const ContentBounds &bounds,
                           dsl::SnapRecord &out);
      bool
      runFlipForwardBack(long tick, scrapbook::MainNode &mainNode, const ContentBounds &bounds, dsl::SnapRecord &out);
      bool runRefusedFlipKeepsPage(long tick,
                                   scrapbook::MainNode &mainNode,
                                   const ContentBounds &bounds,
                                   dsl::SnapRecord &out);
      bool
      runOpenTextPage(long tick, scrapbook::MainNode &mainNode, const ContentBounds &bounds, dsl::SnapRecord &out);
      static PageObservation observePage(const scrapbook::MainNode &mainNode);
      static void setPageObservation(dsl::SnapRecord &record,
                                     const char *pageKey,
                                     const char *captionKey,
                                     const PageObservation &observation);

      Kind kind_;
      std::string name_;
      int stage_;
      PageObservation step1_;
      PageObservation step2_;
    };

    bool IsRegisteredScenario(const std::string &name);
    dsl::SnapRecord MakeDriverErrorRecord(const char *scenario, long errorCode, const char *message);
  } // namespace toolbox_tests
} // namespace loka

#endif // LOKA_TESTS_TOOLBOX_SCRAPBOOK_SCENARIOS_HPP
