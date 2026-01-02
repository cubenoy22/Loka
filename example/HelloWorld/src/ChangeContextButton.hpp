#ifndef DECLARA_HELLOWORLD_CHANGE_CONTEXT_BUTTON_HPP
#define DECLARA_HELLOWORLD_CHANGE_CONTEXT_BUTTON_HPP

#include <cassert>
#include "core2/scene/node/StaticComposition.hpp"
#include "app/Button.hpp"
#include "HelloWorldBoundary.hpp"

namespace helloworld
{
  class ChangeContextButton : public declara::core::scene::StaticCompositionNode
  {
  public:
    ChangeContextButton(const declara::core::scene::StaticCompositionProps &props)
        : declara::core::scene::StaticCompositionNode(props), boundary_(0), toggleEvent_() {}
    virtual ~ChangeContextButton()
    {
      toggleEvent_.unbind(&ChangeContextButton::HandleToggleThunk, this);
    }

    virtual void composeNode(declara::core::scene::NodeComposition &c)
    {
      boundary_ = c.findBoundary<HelloWorldBoundary>();
      assert(boundary_ && "ChangeContextButton requires HelloWorldBoundary");
      toggleEvent_.unbind(&ChangeContextButton::HandleToggleThunk, this);
      toggleEvent_.bind(&ChangeContextButton::HandleToggleThunk, this, false);
      using namespace declara::app;
      ButtonProps props;
      props.setText("Toggle Message");
      props.setOnClick(&toggleEvent_);
      c.declare(Button(props));
    }

  private:
    void handleToggle()
    {
      if (!boundary_)
      {
        return;
      }
      const std::string current = boundary_->messageState().get();
      if (current == "Hello, Declara!")
      {
        boundary_->messageState().set("Context says hi!");
      }
      else
      {
        boundary_->messageState().set("Hello, Declara!");
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
    declara::core::EmitterState toggleEvent_;
  };

  struct ChangeContextButtonDefinition : public declara::core::scene::NodeDefinition<declara::core::scene::StaticCompositionProps, ChangeContextButton>
  {
    ChangeContextButtonDefinition() : NodeDefinition() {}
  };

  inline ChangeContextButtonDefinition ChangeContextButtonNode()
  {
    return ChangeContextButtonDefinition();
  }

} // namespace helloworld

#endif // DECLARA_HELLOWORLD_CHANGE_CONTEXT_BUTTON_HPP
