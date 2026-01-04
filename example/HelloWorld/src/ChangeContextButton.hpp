#ifndef LOKA_HELLOWORLD_CHANGE_CONTEXT_BUTTON_HPP
#define LOKA_HELLOWORLD_CHANGE_CONTEXT_BUTTON_HPP

#include <cassert>
#include <string>
#include "core2/scene/node/Group.hpp"
#include "core/Window.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "app/Button.hpp"
#include "RootBoundary.hpp"

namespace helloworld
{
  class ChangeContextButton;
  typedef declara::core::scene::GroupPropsFor<ChangeContextButton> ChangeContextButtonProps;

  class ChangeContextButton : public declara::core::scene::GroupNodeBase<ChangeContextButtonProps>
  {
  public:
    ChangeContextButton(const ChangeContextButtonProps &props)
        : declara::core::scene::GroupNodeBase<ChangeContextButtonProps>(props), boundary_(0), toggleEvent_() {}
    virtual ~ChangeContextButton()
    {
    }

    virtual void attachNode(declara::core::scene::NodeComposition &c)
    {
      this->boundary_ = c.findBoundary<RootBoundary>();
      assert(this->boundary_ && "ChangeContextButton requires RootBoundary");
      this->bindForUi(toggleEvent_, this, &ChangeContextButton::handleToggle);
    }

    virtual void composeNode(declara::core::scene::NodeComposition &c)
    {
      using namespace declara::app;
      c.declare(Button("Toggle Message", &this->toggleEvent_));
    }

  private:
    void handleToggle()
    {
      const AttachedContext *ctx = this->attachedContext();
      if (!this->boundary_ || !ctx)
      {
        return;
      }
      declara::core::scene::BoundState<std::string> &message = this->boundary_->messageState();
      const std::string current = message.get();
      if (current == "Hello, Loka!")
      {
        message.set("Loka says hi!");
      }
      else
      {
        message.set("Hello, Loka!");
      }

      StateTrackerGuard _(ctx->window()->getTracker());
      MutableState<std::string> &titleState = ctx->window()->titleState();
      const std::string title = titleState.get();
      if (title == "LokaSample")
      {
        titleState.set("LokaSample*");
      }
      else
      {
        titleState.set("LokaSample");
      }
    }

    RootBoundary *boundary_;
    declara::core::EmitterState toggleEvent_;
  };

  typedef declara::core::scene::NodeDefinition<ChangeContextButtonProps, ChangeContextButton> ChangeContextButtonDefinition;

  inline ChangeContextButtonDefinition ChangeContextButtonNode()
  {
    return ChangeContextButtonDefinition();
  }

} // namespace helloworld

#endif // LOKA_HELLOWORLD_CHANGE_CONTEXT_BUTTON_HPP
