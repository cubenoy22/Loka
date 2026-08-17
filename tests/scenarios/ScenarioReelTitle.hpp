#ifndef LOKA_TESTS_SCENARIOS_SCENARIO_REEL_TITLE_HPP
#define LOKA_TESTS_SCENARIOS_SCENARIO_REEL_TITLE_HPP

#include <cstdio>

#include "core/State.hpp"
#include "core/String.hpp"
#include "core/util/StateTrackerGuard.hpp"

namespace loka
{
  namespace scenario_tests
  {
    /** The operator-facing title published by one running reel.

        The reel owns the cell and cycle fact. A presentation may add its
        example-owned production title once, before a Window observes the
        State. Subsequent publications happen only when the reel successfully
        re-arms, through the observing Window owner's tracker transaction. */
    class ScenarioReelTitle
    {
    public:
      ScenarioReelTitle(const char *cell, long completedCycles)
          : productionTitle_(),
            cell_(cell ? core::String::Literal(cell) : core::String()),
            cycleNumber_(completedCycles + 1),
            state_(Format(productionTitle_, cell_, cycleNumber_))
      {
      }

      /** Adds the example-owned title during presentation construction.

          This is a construction-phase operation: call it before passing
          state() to WindowProps. It deliberately updates stored state without
          notifying because no Window or native projection exists yet. */
      void decorateBeforeProjection(const core::String &productionTitle)
      {
        this->productionTitle_ = productionTitle;
        this->state_.setValue(Format(this->productionTitle_, this->cell_, this->cycleNumber_));
      }

      /** Publishes the newly armed cell through the presentation owner's
          tracker. The completed-cycle count becomes the one-based cycle an
          operator sees. */
      void publish(const char *cell, long completedCycles, core::StateTracker *presentationTracker)
      {
        this->cell_ = cell ? core::String::Literal(cell) : core::String();
        this->cycleNumber_ = completedCycles + 1;
        core::StateTrackerGuard guard(presentationTracker);
        this->state_.set(Format(this->productionTitle_, this->cell_, this->cycleNumber_));
      }

      core::MutableState<core::String> *state()
      {
        return &this->state_;
      }

      core::String value() const
      {
        return this->state_.get();
      }

    private:
      static core::String Format(const core::String &productionTitle,
                                 const core::String &cell,
                                 long cycleNumber)
      {
        char cycleText[32];
        ::snprintf(cycleText, sizeof(cycleText), "%ld", cycleNumber);
        if (productionTitle.empty())
        {
          return core::String::Format("%1 (cycle %2)", cell, cycleText);
        }
        // ASCII punctuation stays legible on Classic's byte-string title seam.
        return core::String::Format("%1 - %2 (cycle %3)", productionTitle, cell, cycleText);
      }

      core::String productionTitle_;
      core::String cell_;
      long cycleNumber_;
      core::MutableState<core::String> state_;
    };
  } // namespace scenario_tests
} // namespace loka

#endif // LOKA_TESTS_SCENARIOS_SCENARIO_REEL_TITLE_HPP
