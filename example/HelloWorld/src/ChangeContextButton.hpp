#ifndef LOKA_HELLOWORLD_CHANGE_CONTEXT_BUTTON_HPP
#define LOKA_HELLOWORLD_CHANGE_CONTEXT_BUTTON_HPP

#include "core2/scene/node/Group.hpp"
#include "app/Window.hpp"
#include "loka/core/util/StateTrackerGuard.hpp"
#include "app/Button.hpp"
#include "RootBoundary.hpp"
#include "loka/core/String.hpp"

namespace helloworld
{
  class ChangeContextButton;
  typedef loka::core::scene::GroupPropsFor<ChangeContextButton> ChangeContextButtonProps;

  class ChangeContextButton : public loka::core::scene::GroupNodeBase<ChangeContextButtonProps>
  {
  public:
    ChangeContextButton(const ChangeContextButtonProps &props)
        : loka::core::scene::GroupNodeBase<ChangeContextButtonProps>(props), boundary_(0), toggleEvent_() {}
    virtual ~ChangeContextButton()
    {
    }

    virtual void attachNode(loka::core::scene::NodeComposition &c)
    {
      this->boundary_ = c.findBoundary<RootBoundary>();
      this->bindForUi(toggleEvent_, this, &ChangeContextButton::handleToggle);
    }

    virtual void composeNode(loka::core::scene::NodeComposition &c)
    {
      using namespace loka::app;
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
      loka::core::scene::BoundState<loka::core::String> &message = this->boundary_->messageState();
      const loka::core::String current = message.get();
      if (current.equals(loka::core::String::Literal("Hello, Loka!")))
      {
        message.set(loka::core::String::Literal("Loka says hi!"));
      }
      else
      {
        message.set(loka::core::String::Literal("Hello, Loka!"));
      }

      loka::core::StateTrackerGuard _(ctx->window()->getTracker());
      loka::core::MutableState<loka::core::String> &titleState = ctx->window()->titleState();
      const loka::core::String title = titleState.get();
      if (title.equals(loka::core::String::Literal("LokaSample")))
      {
        titleState.set(loka::core::String::Literal("LokaSample*"));
      }
      else
      {
        titleState.set(loka::core::String::Literal("LokaSample"));
      }
    }

    RootBoundary *boundary_;
    loka::core::EmitterState toggleEvent_;
  };

  typedef loka::core::scene::NodeDefinition<ChangeContextButtonProps, ChangeContextButton> ChangeContextButtonDefinition;

  inline ChangeContextButtonDefinition ChangeContextButtonNode()
  {
    return ChangeContextButtonDefinition();
  }

} // namespace helloworld

#endif // LOKA_HELLOWORLD_CHANGE_CONTEXT_BUTTON_HPP
