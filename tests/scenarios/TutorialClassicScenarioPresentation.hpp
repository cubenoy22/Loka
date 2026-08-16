#ifndef LOKA_TESTS_SCENARIOS_TUTORIAL_CLASSIC_SCENARIO_PRESENTATION_HPP
#define LOKA_TESTS_SCENARIOS_TUTORIAL_CLASSIC_SCENARIO_PRESENTATION_HPP

#include "../../example/Tutorial/src/MyAppConfig.hpp"
#include "ObservedMainDefinition.hpp"

namespace loka
{
  namespace scenario_tests
  {
    /** Presents Tutorial's production window and menu declarations around the
        scene selected by the Classic startup or interaction rail. */
    class TutorialClassicScenarioPresentation : public ::MyAppConfig
    {
    public:
      TutorialClassicScenarioPresentation(PlatformContext *context, bool startup)
          : ::MyAppConfig(context),
            startup_(startup)
      {
      }

      virtual void compose(AppComposition &composition)
      {
        if (this->startup_)
        {
          ObservedMainDefinition<tutorial::DoItYourselfNode::PropsType, tutorial::DoItYourselfNode> mainDefinition(
              tutorial::DoItYourselfNode::PropsType(), 0);
          this->composeObservedWindow(composition, mainDefinition);
          return;
        }
        ObservedMainDefinition<tutorial::Step4Node::PropsType, tutorial::Step4Node> mainDefinition(
            tutorial::Step4Node::PropsType(), 0);
        this->composeObservedWindow(composition, mainDefinition);
      }

    protected:
      bool isStartupPresentation() const
      {
        return this->startup_;
      }

      virtual void onScenarioIdle(Window *window, double elapsedSeconds)
      {
        (void)window;
        (void)elapsedSeconds;
      }

    private:
      void composeObservedWindow(AppComposition &composition, const app::scene::NodeDefinitionBase &mainDefinition)
      {
        composition << WindowDef(this->productionWindowProps(mainDefinition)
                                     .idlePolicy(app::IdlePolicy::everyTick())
                                     .onIdle(&TutorialClassicScenarioPresentation::OnWindowIdle, this));
      }

      static void OnWindowIdle(Window *window, double elapsedSeconds, void *userData)
      {
        TutorialClassicScenarioPresentation *self = static_cast<TutorialClassicScenarioPresentation *>(userData);
        if (self)
        {
          self->onScenarioIdle(window, elapsedSeconds);
        }
      }

      const bool startup_;
    };
  } // namespace scenario_tests
} // namespace loka

#endif // LOKA_TESTS_SCENARIOS_TUTORIAL_CLASSIC_SCENARIO_PRESENTATION_HPP
