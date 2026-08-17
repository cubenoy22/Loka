#include "ScenarioReel.hpp"

#include "app/core/Window.hpp"
#include "app/scene/Scene.hpp"

namespace loka
{
  namespace scenario_tests
  {
    ScenarioReelPosition::ScenarioReelPosition(const ScenarioCellTable &cells, long cycleBudget)
        : cells_(cells),
          cycleBudget_(cycleBudget < 0 ? 0 : cycleBudget),
          index_(0),
          completedCycles_(0)
    {
    }

    const char *ScenarioReelPosition::cell() const
    {
      if (this->exhausted())
      {
        return 0;
      }
      return this->cells_.at(this->index_);
    }

    std::size_t ScenarioReelPosition::index() const
    {
      return this->index_;
    }

    long ScenarioReelPosition::completedCycles() const
    {
      return this->completedCycles_;
    }

    bool ScenarioReelPosition::exhausted() const
    {
      if (this->cells_.empty())
      {
        return true;
      }
      return this->cycleBudget_ > 0 && this->completedCycles_ >= this->cycleBudget_;
    }

    void ScenarioReelPosition::advance()
    {
      if (this->cells_.empty())
      {
        return;
      }
      ++this->index_;
      if (this->index_ >= this->cells_.size())
      {
        this->index_ = 0;
        ++this->completedCycles_;
      }
    }

    bool RearmScenarioScene(Window *window)
    {
      app::scene::Scene *scene = window ? window->scene() : 0;
      if (!scene)
      {
        return false;
      }
      scene->updateAttached(false);
      scene->updateAttached(true);
      return true;
    }
  } // namespace scenario_tests
} // namespace loka
