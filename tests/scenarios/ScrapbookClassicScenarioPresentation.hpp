#ifndef LOKA_TESTS_SCENARIOS_SCRAPBOOK_CLASSIC_SCENARIO_PRESENTATION_HPP
#define LOKA_TESTS_SCENARIOS_SCRAPBOOK_CLASSIC_SCENARIO_PRESENTATION_HPP

#include "../../example/ScrapbookUI/src/MyAppConfig.hpp"
#include "ObservedMainDefinition.hpp"

namespace loka
{
  namespace scenario_tests
  {
    /** Presents ScrapbookUI's production window and menu declarations while
        retaining the observed main node required by the Classic rail. */
    class ScrapbookClassicScenarioPresentation : public ::ScrapbookAppConfig
    {
    public:
      explicit ScrapbookClassicScenarioPresentation(PlatformContext *context)
          : ::ScrapbookAppConfig(context),
            borrowedMainNode_(0)
      {
      }

      virtual void compose(AppComposition &composition)
      {
        ObservedMainDefinition<scrapbook::MainProps, scrapbook::MainNode> mainDefinition(
            scrapbook::MainProps().platformContext(this->getPlatformContext()), &this->borrowedMainNode_);
        composition << WindowDef(this->productionWindowProps(mainDefinition)
                                     .idlePolicy(app::IdlePolicy::everyTick())
                                     .onIdle(&ScrapbookClassicScenarioPresentation::OnWindowIdle, this));
      }

    protected:
      scrapbook::MainNode *observedMainNode() const
      {
        return this->borrowedMainNode_;
      }

      virtual void onScenarioIdle(Window *window, double elapsedSeconds)
      {
        (void)window;
        (void)elapsedSeconds;
      }

    private:
      static void OnWindowIdle(Window *window, double elapsedSeconds, void *userData)
      {
        ScrapbookClassicScenarioPresentation *self = static_cast<ScrapbookClassicScenarioPresentation *>(userData);
        if (self)
        {
          self->onScenarioIdle(window, elapsedSeconds);
        }
      }

      scrapbook::MainNode *borrowedMainNode_;
    };
  } // namespace scenario_tests
} // namespace loka

#endif // LOKA_TESTS_SCENARIOS_SCRAPBOOK_CLASSIC_SCENARIO_PRESENTATION_HPP
