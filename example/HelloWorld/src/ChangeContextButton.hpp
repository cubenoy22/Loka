#ifndef DECLARA_HELLOWORLD_CHANGE_CONTEXT_BUTTON_HPP
#define DECLARA_HELLOWORLD_CHANGE_CONTEXT_BUTTON_HPP

#include "core2/scene/node/StaticComposition.hpp"
#include "app/Button.hpp"
#include "HelloWorldContext.hpp"

namespace helloworld
{
  class ChangeContextButton : public declara::core::scene::StaticCompositionNode
  {
  public:
    ChangeContextButton(const declara::core::scene::StaticCompositionProps &props)
        : declara::core::scene::StaticCompositionNode(props), context_(0), toggleEvent_() {}
    virtual ~ChangeContextButton()
    {
      toggleEvent_.unbind(&ChangeContextButton::HandleToggleThunk, this);
    }

    virtual void composeNode(declara::core::scene::NodeComposition &c)
    {
      context_ = &this->useContext(HelloWorldContextDefinition(), declara::core::scene::CONTEXT_PLACEMENT_BOUNDARY);
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
      HelloWorldContext *ctx = context_;
      if (!ctx)
      {
        ctx = &this->useContext(HelloWorldContextDefinition(), declara::core::scene::CONTEXT_PLACEMENT_BOUNDARY);
      }
      const std::string current = ctx->message.get();
      if (current == "Hello, Declara!")
      {
        ctx->message.set("Context says hi!");
      }
      else
      {
        ctx->message.set("Hello, Declara!");
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

    HelloWorldContext *context_;
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
