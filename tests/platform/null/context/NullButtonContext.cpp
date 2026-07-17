#include "platform/null/context/NullButtonContext.hpp"

#include "app/nodes/controls/Button.hpp"
#include "app/scene/projection/PlatformNodeHandler.hpp"

namespace
{
  class NullButtonNodeHandler : public loka::app::scene::IPlatformNodeHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<loka::app::ButtonNode>();
    }

    virtual loka::app::scene::NodeContext *ensureContext(loka::app::scene::Node *node,
                                                         loka::app::scene::IPlatformController *controller,
                                                         const loka::app::scene::LayoutState &state)
    {
      (void)state;
      loka::app::ButtonNode *button = node ? node->asButtonNode() : 0;
      NullScenePlatformController *nullPlatform =
          static_cast<NullScenePlatformController *>(controller);
      if (!button || !nullPlatform)
      {
        return 0;
      }
      NullButtonContext *context = static_cast<NullButtonContext *>(button->getContext());
      if (!context)
      {
        context = new NullButtonContext(button, nullPlatform);
        if (!context)
        {
          return 0;
        }
        button->setContext(context);
        context->readLifecycleFactOnAttach();
      }
      return context;
    }
  };

  NullButtonNodeHandler gNullButtonNodeHandler;
} // namespace

NullButtonContext::NullButtonContext(loka::app::ButtonNode *node,
                                     NullScenePlatformController *controller)
    : loka::app::scene::NativeNodeContext(),
      node_(node),
      controller_(controller),
      handle_(0)
{
  if (this->controller_ && this->node_)
  {
    this->handle_ = this->controller_->createLedgerRow(
        NullScenePlatformController::CONTROL_RECIPE_BUTTON, this, this->node_->nativeLifetimeHint());
  }
}

NullButtonContext::~NullButtonContext()
{
  if (this->controller_)
  {
    this->controller_->completeContextTeardown(this->handle_);
  }
  this->handle_ = 0;
  this->controller_ = 0;
  this->node_ = 0;
}

void NullButtonContext::readLifecycleFactOnAttach()
{
  if (this->controller_ && this->node_)
  {
    this->controller_->setVisible(this->handle_,
                                  this->node_->lifecycleFact() == loka::app::scene::NODE_FACT_ATTACHED);
    this->controller_->observeHint(this->handle_, this->lifetimeHint());
  }
}

void NullButtonContext::onFactChanged(loka::app::scene::NodeLifecycleFact previous,
                                      loka::app::scene::NodeLifecycleFact next)
{
  (void)previous;
  if (this->controller_)
  {
    this->controller_->setVisible(this->handle_, next == loka::app::scene::NODE_FACT_ATTACHED);
    this->controller_->observeHint(this->handle_, this->lifetimeHint());
  }
}

short NullButtonContext::layout(loka::app::scene::IPlatformController *,
                                loka::app::scene::LayoutState &state)
{
  return static_cast<short>(state.y + state.height);
}

void RegisterNullButtonNodeHandler(NullScenePlatformController &controller)
{
  controller.registerNodeHandler(&gNullButtonNodeHandler);
}
