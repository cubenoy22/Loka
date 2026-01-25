#include "MainNode.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "core/Window.hpp"
#include "app/Button.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"

namespace helloworld
{
  MainLeftPanelComponent::MainLeftPanelComponent(MainNode *owner)
      : owner_(owner), initialized_(false), message_(), toggleEvent_(), bmiCalculator_()
  {
  }

  void MainLeftPanelComponent::attachNode(loka::core::scene::NodeComposition &c)
  {
    if (initialized_)
    {
      return;
    }
    c.declareStates()
        .state(message_, loka::core::String::Literal("Hello, Loka!"));
    if (owner_)
    {
      owner_->bindForUiComponent(toggleEvent_, this, &MainLeftPanelComponent::toggleMessage);
    }
    initialized_ = true;
  }

  void MainLeftPanelComponent::composeNode(loka::core::scene::NodeComposition &c)
  {
    using namespace loka::app;
    if (!owner_)
    {
      return;
    }
    c.declare(
        VStack()
        << Text("Loka Sample")
        << Text(message_)
        << Button("Add +", &toggleEvent_)
        << loka::core::scene::LC(bmiCalculator_));
  }

  void MainLeftPanelComponent::toggleMessage()
  {
    if (!message_.isValid())
    {
      return;
    }
    if (!owner_)
    {
      return;
    }
    ::Window *window = owner_->windowOrNull();
    loka::core::String next = message_.get() + " +Loka";
    message_.set(next, true);

    if (!window)
    {
      return;
    }
    {
      StateTrackerGuard _(window->getTracker());
      const loka::core::String title = window->titleState().get();

      if (title.equals(loka::core::String::Literal("LokaSample")))
      {
        window->titleState().set(loka::core::String::Literal("LokaSample*"));
      }
      else
      {
        window->titleState().set(loka::core::String::Literal("LokaSample"));
      }
    }
  }
} // namespace helloworld
