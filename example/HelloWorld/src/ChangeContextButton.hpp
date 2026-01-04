#ifndef DECLARA_HELLOWORLD_CHANGE_CONTEXT_BUTTON_HPP
#define DECLARA_HELLOWORLD_CHANGE_CONTEXT_BUTTON_HPP

#include <cassert>
#include <string>
#include "core2/scene/node/Group.hpp"
#include "core2/scene/node/Boundary.hpp"
#include "core/Window.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "app/Button.hpp"
#include "HelloWorldBoundary.hpp"

namespace helloworld
{
  class ChangeContextButton;
  typedef declara::core::scene::GroupPropsFor<ChangeContextButton> ChangeContextButtonProps;

  class ChangeContextButton : public declara::core::scene::GroupNodeBase<ChangeContextButtonProps>
  {
  public:
    ChangeContextButton(const ChangeContextButtonProps &props)
        : declara::core::scene::GroupNodeBase<ChangeContextButtonProps>(props), boundary_(0), boundaryNode_(0), toggleEvent_() {}
    virtual ~ChangeContextButton()
    {
    }

    virtual void prepareNode(declara::core::scene::NodeComposition &c)
    {
      this->boundary_ = c.findBoundary<HelloWorldBoundary>();
      this->boundaryNode_ = c.findBoundary<declara::core::scene::BoundaryNode>();
      assert(this->boundary_ && "ChangeContextButton requires HelloWorldBoundary");
      this->toggleEvent_.bind(&ChangeContextButton::HandleToggleThunk, this, false);
    }

    virtual void composeNode(declara::core::scene::NodeComposition &c)
    {
      using namespace declara::app;
      ButtonProps props;
      props.text("Toggle Message");
      props.onClick(&this->toggleEvent_);
      c.declare(Button(props));
    }

  private:
    void handleToggle()
    {
      if (!this->boundary_)
      {
        return;
      }
      const std::string current = this->boundary_->messageState().get();
      if (current == "Hello, Loka!")
      {
        this->boundary_->messageState().set("Loka says hi!");
      }
      else
      {
        this->boundary_->messageState().set("Hello, Loka!");
      }

      if (!boundaryNode_)
      {
        return;
      }
      declara::core::scene::Scene *scene = this->boundaryNode_->getScene();
      if (!scene)
      {
        return;
      }
      Window *window = scene->getWindow();
      if (!window)
      {
        return;
      }
      StateTrackerGuard _(window->getTracker());
      const std::string title = window->titleState().get();
      if (title == "LokaSample")
      {
        window->titleState().set("LokaSample*");
      }
      else
      {
        window->titleState().set("LokaSample");
      }
    }

    static void HandleToggleThunk(void *userData)
    {
      ChangeContextButton *self = static_cast<ChangeContextButton *>(userData);
      if (self)
      {
        self->handleToggle();
      }
    }

    HelloWorldBoundary *boundary_;
    declara::core::scene::BoundaryNode *boundaryNode_;
    declara::core::EmitterState toggleEvent_;
  };

  typedef declara::core::scene::NodeDefinition<ChangeContextButtonProps, ChangeContextButton> ChangeContextButtonDefinition;

  inline ChangeContextButtonDefinition ChangeContextButtonNode()
  {
    return ChangeContextButtonDefinition();
  }

} // namespace helloworld

#endif // DECLARA_HELLOWORLD_CHANGE_CONTEXT_BUTTON_HPP
