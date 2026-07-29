#include "platform/null/context/NullScrollBarContext.hpp"

#include "app/scene/projection/PlatformNodeHandler.hpp"

namespace
{
  class NullScrollBarNodeHandler : public loka::app::scene::IPlatformNodeHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<loka::app::ScrollBarNode>();
    }

    virtual loka::app::scene::NodeContext *ensureContext(loka::app::scene::Node *node,
                                                         loka::app::scene::IPlatformController *controller,
                                                         const loka::app::scene::LayoutState &state)
    {
      (void)state;
      loka::app::ScrollBarNode *scrollBar = node ? node->asScrollBarNode() : 0;
      NullScenePlatformController *nullPlatform =
          static_cast<NullScenePlatformController *>(controller);
      if (!scrollBar || !nullPlatform)
      {
        return 0;
      }
      NullScrollBarContext *context = static_cast<NullScrollBarContext *>(scrollBar->getContext());
      if (!context)
      {
        context = new NullScrollBarContext(scrollBar, nullPlatform);
        if (!context)
        {
          return 0;
        }
        scrollBar->setContext(context);
        context->readLifecycleFactOnAttach();
        return context;
      }
      // Range and value are re-applied on every projection, not only at
      // creation: `range()` is static prop data, so a recomposed range only
      // reaches the native side through a projection sweep.
      context->syncFromNode();
      return context;
    }
  };

  NullScrollBarNodeHandler gNullScrollBarNodeHandler;
} // namespace

NullScrollBarContext::NullScrollBarContext(loka::app::ScrollBarNode *node,
                                           NullScenePlatformController *controller)
    : loka::app::scene::NativeNodeContext(),
      node_(node),
      controller_(controller),
      handle_(0),
      trackedValue_(0),
      appliedValue_(0),
      stateWriteCount_(0)
{
  if (this->controller_ && this->node_)
  {
    this->handle_ = this->controller_->createLedgerRow(
        NullScenePlatformController::CONTROL_RECIPE_SCROLL_BAR, this, this->node_->nativeLifetimeHint());
  }
  this->syncFromNode();
}

NullScrollBarContext::~NullScrollBarContext()
{
  if (this->controller_)
  {
    this->controller_->completeContextTeardown(this->handle_);
  }
  this->handle_ = 0;
  this->controller_ = 0;
  this->node_ = 0;
}

void NullScrollBarContext::readLifecycleFactOnAttach()
{
  if (this->controller_ && this->node_)
  {
    this->controller_->setVisible(this->handle_,
                                  this->node_->lifecycleFact() == loka::app::scene::NODE_FACT_ATTACHED);
    this->controller_->observeHint(this->handle_, this->lifetimeHint());
  }
}

void NullScrollBarContext::onFactChanged(loka::app::scene::NodeLifecycleFact previous,
                                         loka::app::scene::NodeLifecycleFact next)
{
  (void)previous;
  if (this->controller_)
  {
    this->controller_->setVisible(this->handle_, next == loka::app::scene::NODE_FACT_ATTACHED);
    this->controller_->observeHint(this->handle_, this->lifetimeHint());
  }
}

short NullScrollBarContext::layout(loka::app::scene::IPlatformController *,
                                   loka::app::scene::LayoutState &state)
{
  this->syncFromNode();
  return static_cast<short>(state.y + state.height);
}

void NullScrollBarContext::syncFromNode()
{
  if (!this->node_)
  {
    return;
  }
  this->trackedValue_ = this->node_->displayValue();
  this->appliedValue_ = this->trackedValue_;
}

int NullScrollBarContext::displayedValue() const
{
  return this->trackedValue_;
}

int NullScrollBarContext::minimum() const
{
  return this->node_ ? this->node_->props.min_ : 0;
}

int NullScrollBarContext::maximum() const
{
  return this->node_ ? this->node_->props.max_ : 0;
}

int NullScrollBarContext::lineStep() const
{
  return this->node_ ? this->node_->props.lineStep_ : 0;
}

int NullScrollBarContext::pageStep() const
{
  return this->node_ ? this->node_->props.pageStep_ : 0;
}

loka::app::ScrollBarOrientation NullScrollBarContext::orientation() const
{
  return this->node_ ? this->node_->props.orientation_ : loka::app::SCROLL_BAR_VERTICAL;
}

bool NullScrollBarContext::active() const
{
  return this->node_ && this->node_->isActive();
}

unsigned long NullScrollBarContext::stateWriteCount() const
{
  return this->stateWriteCount_;
}

void NullScrollBarContext::pressTick(Part part)
{
  if (!this->active())
  {
    return;
  }
  const bool isLine = part == PART_LINE_UP || part == PART_LINE_DOWN;
  const int step = isLine ? this->lineStep() : this->pageStep();
  const int direction = (part == PART_LINE_UP || part == PART_PAGE_UP) ? -1 : 1;
  // One action-proc tick: moves only what the user sees. Nothing crosses
  // into Loka here -- a held arrow over a page-flipping binding would
  // otherwise fire one read per tick (ruling 1).
  this->trackedValue_ =
      loka::app::ScrollBarClampValue(this->trackedValue_ + direction * step, this->minimum(), this->maximum());
}

void NullScrollBarContext::dragThumbTo(int value)
{
  if (!this->active())
  {
    return;
  }
  this->trackedValue_ = loka::app::ScrollBarClampValue(value, this->minimum(), this->maximum());
}

void NullScrollBarContext::release()
{
  if (!this->active())
  {
    return;
  }
  this->commitTrackedValue();
}

void NullScrollBarContext::simulatePress(Part part, int repeatWhileHeld)
{
  if (repeatWhileHeld < 1)
  {
    repeatWhileHeld = 1;
  }
  for (int i = 0; i < repeatWhileHeld; ++i)
  {
    this->pressTick(part);
  }
  this->release();
}

void NullScrollBarContext::simulateThumbDragTo(int value)
{
  this->dragThumbTo(value);
  this->release();
}

void NullScrollBarContext::commitTrackedValue()
{
  if (!this->node_)
  {
    return;
  }
  if (this->trackedValue_ == this->appliedValue_)
  {
    // The gesture settled back where it started -- a cancelled drag, or an
    // arrow held against the end of the range. Publishing it anyway would
    // fire onChange for a gesture that changed nothing (the Toolbox arm's
    // appliedValue gate, mirrored so both arms hold one contract).
    return;
  }
  loka::core::State<int> *value = this->node_->props.value_;
  if (!value)
  {
    return;
  }
  loka::core::MutableState<int> *mutableValue =
      static_cast<loka::core::MutableState<int> *>(value->asMutableState());
  if (!mutableValue)
  {
    return;
  }
  // Order is part of the contract: the settled value lands in the binding
  // first, so an onChange handler reads it from the State it already holds
  // instead of needing an event payload.
  ++this->stateWriteCount_;
  this->appliedValue_ = this->trackedValue_;
  mutableValue->set(this->trackedValue_, true);
  if (this->node_ && this->node_->props.onChange_)
  {
    this->node_->props.onChange_->emit();
  }
}

void RegisterNullScrollBarNodeHandler(NullScenePlatformController &controller)
{
  controller.registerNodeHandler(&gNullScrollBarNodeHandler);
}
