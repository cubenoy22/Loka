#ifndef LOKA_TESTS_SCENARIOS_SCENARIO_REEL_HPP
#define LOKA_TESTS_SCENARIOS_SCENARIO_REEL_HPP

#include <cstddef>
#include <new>
#include <string>

#include "ScenarioCellTable.hpp"
#include "ScenarioTypes.hpp"
#include "SceneScenarioDriver.hpp"
#include "StartupScenarios.hpp"
#include "core/util/ScopedPtr.hpp"

class Window;

namespace loka
{
  namespace scenario_tests
  {
    /** Where a reel stands and how many full passes it has completed.

        Separated from the running machinery because it is the only part with
        no scene, no rail, and no clock: a cell index, a wrap, and a budget.
        A budget of 0 is the shipping case and never ends; a positive budget is
        the bounded verification knob, so a soak run can be asserted and stop
        instead of being killed from outside. */
    class ScenarioReelPosition
    {
    public:
      ScenarioReelPosition(const ScenarioCellTable &cells, long cycleBudget);

      /** The current cell name, or 0 when the reel has no cells left to run. */
      const char *cell() const;
      std::size_t index() const;
      long completedCycles() const;
      /** True once a bounded budget is spent, or when the table is empty. */
      bool exhausted() const;
      /** Steps to the next cell, counting one completed pass at the wrap. */
      void advance();

    private:
      ScenarioCellTable cells_;
      const long cycleBudget_;
      std::size_t index_;
      long completedCycles_;
    };

    /** Re-arms a mounted scene by running the framework's own detach and
        rebuild pair, the same pair SceneManager::swapScene runs across a scene
        transition: the detach tears the composed tree down, drains the live
        Boundary retire queues, releases every native node context, calls
        IPlatformController::destroy() and destroys the root node; the
        re-attach builds a fresh root node from the scene's root definition and
        recomposes it. Nothing is reset in place, so the example's state is new
        because its nodes are new.

        Returns false when the window has no mounted scene to re-arm. */
    bool RearmScenarioScene(Window *window);

    /** Runs one example's registered cells endlessly in one process: step the
        current cell to its terminal record, hold the settled scene, then
        re-arm and start the next cell, wrapping at the end of the table.

        The retired rail is destroyed rather than rewound. Its terminal state
        is a one-way latch and its audit emitter refuses a second emission by
        design, so a loop that reset either of them would be running a
        different mechanism than a fresh launch does. Rail destruction happens
        before the scene teardown because the rail's Flow observes nodes the
        teardown destroys. */
    template <class InteractionScenario> class ScenarioReel
    {
    public:
      ScenarioReel(const ScenarioCellTable &cells,
                   StartupExample startupExample,
                   DriverErrorFactory errorFactory,
                   long interactionErrorCode,
                   double holdSeconds,
                   long cycleBudget)
          : position_(cells, cycleBudget),
            startupExample_(startupExample),
            errorFactory_(errorFactory),
            interactionErrorCode_(interactionErrorCode),
            holdSeconds_(holdSeconds),
            driver_(0),
            tick_(0),
            holdRemaining_(0.0),
            holding_(false)
      {
        this->arm();
      }

      /** One idle tick from the platform pump. */
      void tick(Window *window, double elapsedSeconds)
      {
        if (this->position_.exhausted() || !this->driver_.get())
        {
          return;
        }
        if (this->holding_)
        {
          this->holdRemaining_ -= elapsedSeconds;
          if (this->holdRemaining_ > 0.0)
          {
            return;
          }
          this->rearm(window);
          return;
        }
        ++this->tick_;
        dsl::SnapRecord record;
        const CaptureContentBounds bounds;
        switch (this->driver_->step(this->tick_, window, bounds, record))
        {
        case SCENARIO_ADVANCE_PENDING:
          break;
        case SCENARIO_ADVANCE_DRIVER_COMPLETION_READY:
        case SCENARIO_ADVANCE_FINAL_SCENE_HELD:
          this->holding_ = true;
          this->holdRemaining_ = this->holdSeconds_;
          break;
        }
      }

      /** True once a bounded budget is spent; always false for a shipping reel. */
      bool finished() const
      {
        return this->position_.exhausted();
      }

      long completedCycles() const
      {
        return this->position_.completedCycles();
      }

      const char *cell() const
      {
        return this->position_.cell();
      }

    private:
      void rearm(Window *window)
      {
        this->position_.advance();
        // The rail goes first: its Flow observes nodes the teardown destroys.
        this->driver_.reset(0);
        if (this->position_.exhausted())
        {
          return;
        }
        (void)RearmScenarioScene(window);
        this->arm();
      }

      void arm()
      {
        const char *name = this->position_.cell();
        this->tick_ = 0;
        this->holding_ = false;
        this->holdRemaining_ = 0.0;
        if (!name)
        {
          return;
        }
        this->driver_.reset(new (std::nothrow) SceneScenarioDriver<InteractionScenario>(
            IsStartupScenario(name),
            this->startupExample_,
            std::string(name),
            this->errorFactory_,
            this->interactionErrorCode_,
            0));
      }

      ScenarioReelPosition position_;
      const StartupExample startupExample_;
      DriverErrorFactory errorFactory_;
      const long interactionErrorCode_;
      const double holdSeconds_;
      core::ScopedPtr<SceneScenarioDriver<InteractionScenario> > driver_;
      long tick_;
      double holdRemaining_;
      bool holding_;

      ScenarioReel(const ScenarioReel &);
      ScenarioReel &operator=(const ScenarioReel &);
    };
  } // namespace scenario_tests
} // namespace loka

#endif // LOKA_TESTS_SCENARIOS_SCENARIO_REEL_HPP
