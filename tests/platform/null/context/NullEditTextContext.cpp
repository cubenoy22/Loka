#include "platform/null/context/NullEditTextContext.hpp"

#include "app/nodes/controls/EditText.hpp"
#include "app/scene/projection/PlatformNodeHandler.hpp"

namespace
{
  class NullEditTextNodeHandler : public loka::app::scene::IPlatformNodeHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<loka::app::EditTextNode>();
    }

    virtual loka::app::scene::NodeContext *ensureContext(loka::app::scene::Node *node,
                                                         loka::app::scene::IPlatformController *controller,
                                                         const loka::app::scene::LayoutState &state)
    {
      (void)state;
      loka::app::EditTextNode *editText = node ? node->asEditTextNode() : 0;
      NullScenePlatformController *nullPlatform =
          static_cast<NullScenePlatformController *>(controller);
      if (!editText || !nullPlatform)
      {
        return 0;
      }
      NullEditTextContext *context = static_cast<NullEditTextContext *>(editText->getContext());
      if (!context)
      {
        context = new NullEditTextContext(editText, nullPlatform);
        if (!context)
        {
          return 0;
        }
        editText->setContext(context);
      }
      context->observeNodeLifetimeHint();
      return context;
    }
  };

  NullEditTextNodeHandler gNullEditTextNodeHandler;
} // namespace

NullEditTextContext::NullEditTextContext(loka::app::EditTextNode *node,
                                         NullScenePlatformController *controller)
    : loka::app::scene::NativeNodeContext(),
      node_(node),
      controller_(controller),
      handle_(0)
{
  if (this->controller_ && this->node_)
  {
    this->observeLifetimeHint(this->node_->nativeLifetimeHint());
    this->handle_ = this->controller_->createLedgerRow(
        NullScenePlatformController::CONTROL_RECIPE_EDIT_TEXT, this, this->lifetimeHint());
  }
}

NullEditTextContext::~NullEditTextContext()
{
  if (this->controller_)
  {
    this->controller_->completeContextTeardown(this->handle_);
  }
  this->handle_ = 0;
  this->controller_ = 0;
  this->node_ = 0;
}

void NullEditTextContext::onNodeAttached()
{
  if (this->controller_)
  {
    this->controller_->setVisible(this->handle_, true);
  }
}

void NullEditTextContext::onNodeDetached()
{
  if (this->controller_)
  {
    this->controller_->setVisible(this->handle_, false);
  }
}

void NullEditTextContext::onFactChanged(loka::app::scene::NodeLifecycleFact previous,
                                        loka::app::scene::NodeLifecycleFact next)
{
  (void)previous;
  if (this->controller_)
  {
    this->controller_->setVisible(this->handle_, next == loka::app::scene::NODE_FACT_ATTACHED);
  }
}

short NullEditTextContext::layout(loka::app::scene::IPlatformController *,
                                  loka::app::scene::LayoutState &state)
{
  this->observeNodeLifetimeHint();
  return static_cast<short>(state.y + state.height);
}

void NullEditTextContext::observeNodeLifetimeHint()
{
  if (!this->node_)
  {
    return;
  }
  this->observeLifetimeHint(this->node_->nativeLifetimeHint());
  if (this->controller_)
  {
    this->controller_->observeHint(this->handle_, this->lifetimeHint());
  }
}

void RegisterNullEditTextNodeHandler(NullScenePlatformController &controller)
{
  controller.registerNodeHandler(&gNullEditTextNodeHandler);
}
