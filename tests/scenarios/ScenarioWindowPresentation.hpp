#ifndef LOKA_TESTS_SCENARIOS_SCENARIO_WINDOW_PRESENTATION_HPP
#define LOKA_TESTS_SCENARIOS_SCENARIO_WINDOW_PRESENTATION_HPP

#include "ScenarioReelTitle.hpp"
#include "app/core/Window.hpp"

namespace loka
{
  namespace scenario_tests
  {
    /** Optional presentation-only projection for a scenario Window.

        Ordinary scenario rails leave the borrowed reel title empty and keep
        the example's production WindowProps unchanged. ScenarioLoopAppConfig
        installs the title owned by its reel; this seam then decorates the
        local presentation props without changing the example declaration. */
    class ScenarioWindowPresentation
    {
    protected:
      ScenarioWindowPresentation()
          : reelTitle_(0)
      {
      }

      void presentScenarioReelTitle(ScenarioReelTitle *reelTitle)
      {
        this->reelTitle_ = reelTitle;
      }

      void applyScenarioWindowPresentation(WindowProps &props)
      {
        if (!this->reelTitle_)
        {
          return;
        }
        const core::String productionTitle = props.hasInitialTitle ? props.initialTitle : core::String();
        this->reelTitle_->decorateBeforeProjection(productionTitle);
        props.titleState(this->reelTitle_->state()).title(this->reelTitle_->value());
      }

    private:
      // Borrowed from the derived loop config; its reel outlives the Window.
      ScenarioReelTitle *reelTitle_;
    };
  } // namespace scenario_tests
} // namespace loka

#endif // LOKA_TESTS_SCENARIOS_SCENARIO_WINDOW_PRESENTATION_HPP
