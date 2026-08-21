#ifndef LOKA_TESTS_SCENARIOS_HELLO_WORLD_PRODUCTION_FRAME_HPP
#define LOKA_TESTS_SCENARIOS_HELLO_WORLD_PRODUCTION_FRAME_HPP

#include "../../example/HelloWorld/src/MainNode.hpp"
#include "../../example/HelloWorld/src/ProductionAppConfig.hpp"
#include "app/core/Window.hpp"

namespace loka
{
  namespace scenario_tests
  {
    /** HelloWorld's shipping window size, read from the production config.

        Headless cells capture at this size rather than repeating the literal,
        so shrinking the production frame moves crop_bottom in every audit the
        cells produce and the tracked expectations go red. Before this existed,
        reverting the production height alone left all 413 tests green: the
        cells carried their own copy of the number and the presentation vehicle
        compares two configurations that both read the production one.

        What is pinned here is the size, not what fits inside it. Headless
        layout has no real font metrics -- that is why #404's clipping was
        invisible to CTest in the first place -- so the rails remain the only
        place that can see whether the content clears the fold. */
    class HelloWorldProductionFrame : private HelloWorldProductionAppConfig
    {
    public:
      HelloWorldProductionFrame()
          : HelloWorldProductionAppConfig(0, HelloWorldMenuSeed::FromWallClock(0))
      {
      }

      int width() const
      {
        return this->productionWindowProps(
                       loka::app::scene::Boundary<helloworld::MainNode>())
            .width;
      }

      int height() const
      {
        return this->productionWindowProps(
                       loka::app::scene::Boundary<helloworld::MainNode>())
            .height;
      }
    };
  } // namespace scenario_tests
} // namespace loka

#endif // LOKA_TESTS_SCENARIOS_HELLO_WORLD_PRODUCTION_FRAME_HPP
