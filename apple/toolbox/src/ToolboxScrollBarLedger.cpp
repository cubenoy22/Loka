#include "ToolboxScrollBarLedger.hpp"
#include "ToolboxScenePlatformController.hpp"
#include "ToolboxScrollViewDecisions.hpp"
#include "ToolboxWindow.hpp"
#include "app/nodes/controls/ScrollBar.hpp"
#include "app/nodes/nestable/ScrollView.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include <Controls.h>

namespace
{
  // The CDEF id and its part codes live in ControlDefinitions.h, which this
  // toolchain's Controls.h does not pull in. Same Universal Interfaces values.
#if !defined(scrollBarProc) && !defined(LOKA_TOOLBOX_MULTIVERSAL_INTERFACES)
  enum
  {
    scrollBarProc = 16
  };
#endif
#if !defined(kControlUpButtonPart)
  enum
  {
    kControlUpButtonPart = 20,
    kControlDownButtonPart = 21,
    kControlPageUpPart = 22,
    kControlPageDownPart = 23
  };
#endif
} // namespace

namespace
{
  // The action proc runs inside TrackControl and has no user-data slot, so
  // the step sizes of the bar being tracked are parked here for the duration
  // of the loop. Classic is single-threaded and TrackControl does not nest,
  // so exactly one bar is ever being tracked.
  int gActiveScrollBarLineStep = 1;
  int gActiveScrollBarPageStep = 1;
  ControlActionUPP gScrollBarActionUPP = 0;

  static pascal void ScrollBarActionProc(ControlRef control, ControlPartCode part)
  {
    if (!control)
    {
      return;
    }
    const int minimum = static_cast<int>(GetControlMinimum(control));
    const int maximum = static_cast<int>(GetControlMaximum(control));
    int value = static_cast<int>(GetControlValue(control));
    switch (part)
    {
    case kControlUpButtonPart:
      value -= gActiveScrollBarLineStep;
      break;
    case kControlDownButtonPart:
      value += gActiveScrollBarLineStep;
      break;
    case kControlPageUpPart:
      value -= gActiveScrollBarPageStep;
      break;
    case kControlPageDownPart:
      value += gActiveScrollBarPageStep;
      break;
    default:
      return;
    }
    value = loka::app::ScrollBarClampValue(value, minimum, maximum);
    // Visual only. Nothing crosses into Loka from inside the loop: a held
    // arrow must not publish one value per tick (ruling 1).
    SetControlValue(control, static_cast<short>(value));
  }

  ControlActionUPP ScrollBarActionUPP()
  {
    if (!gScrollBarActionUPP)
    {
      gScrollBarActionUPP = NewControlActionUPP(ScrollBarActionProc);
    }
    return gScrollBarActionUPP;
  }
} // namespace

bool ToolboxScenePlatformController::ensureScrollBarBinding(
    short resourceId,
    const Rect &rect,
    int minimum,
    int maximum,
    int lineStep,
    int pageStep,
    loka::app::scene::NativeLifetimeHint lifetimeHint,
    ScrollBarControlBinding *&binding)
{
  binding = 0;
  if (!window_ || !window_->window() || resourceId <= 0)
  {
    return false;
  }
  Rect controlRect;
  if (!this->intersectWithProjectionClip(rect, controlRect))
  {
    return true;
  }
  for (size_t i = 0; i < scrollBarLedger_.scrollBarControls_.size(); ++i)
  {
    if (scrollBarLedger_.scrollBarControls_[i].resourceId == resourceId)
    {
      binding = &scrollBarLedger_.scrollBarControls_[i];
      break;
    }
  }
  bool created = false;
  if (!binding)
  {
    ControlRef control = 0;
    if (!scrollBarLedger_.scrollBarBucket_.tryAcquire(control))
    {
      Rect rectCopy = controlRect;
      Str255 title;
      title[0] = 0;
      // scrollBarProc is the pre-Appearance standard CDEF: available on
      // every system this arm targets, and the one the Classic look expects.
      control = NewControl(window_->window(), &rectCopy, title, false, 0, 0, 1, scrollBarProc, 0);
      if (!control)
      {
        return false;
      }
      HideControl(control);
    }
    ScrollBarControlBinding entry;
    entry.resourceId = resourceId;
    entry.control = control;
    entry.value = 0;
    entry.onChange = 0;
    entry.enabled = 0;
    entry.minimum = 0;
    entry.maximum = 0;
    entry.lineStep = 1;
    entry.pageStep = 1;
    entry.appliedValue = 0;
    entry.active = false;
    entry.usedThisFrame = true;
    entry.rect = controlRect;
    entry.lifetimeHint = lifetimeHint;
    scrollBarLedger_.scrollBarControls_.push_back(entry);
    binding = &scrollBarLedger_.scrollBarControls_.back();
    created = true;
  }
  binding->minimum = minimum;
  binding->maximum = maximum;
  binding->lineStep = lineStep;
  binding->pageStep = pageStep;
  binding->lifetimeHint = lifetimeHint;
  binding->usedThisFrame = true;
  if (created || binding->rect.left != controlRect.left || binding->rect.top != controlRect.top
      || binding->rect.right != controlRect.right || binding->rect.bottom != controlRect.bottom)
  {
    MoveControl(binding->control, controlRect.left, controlRect.top);
    SizeControl(binding->control, controlRect.right - controlRect.left, controlRect.bottom - controlRect.top);
    binding->rect = controlRect;
  }
  SetControlMinimum(binding->control, static_cast<short>(minimum));
  SetControlMaximum(binding->control, static_cast<short>(maximum));
  return true;
}

bool ToolboxScenePlatformController::ensureScrollBarControl(short resourceId,
                                                             const Rect &rect,
                                                             const loka::app::ScrollBarProps &props,
                                                             loka::app::scene::NativeLifetimeHint lifetimeHint)
{
  ScrollBarControlBinding *binding = 0;
  if (!this->ensureScrollBarBinding(
          resourceId, rect, props.min_, props.max_,
          props.lineStep_, props.pageStep_, lifetimeHint, binding))
  {
    return false;
  }
  if (!binding)
  {
    return true;
  }
  binding->value = props.value_;
  binding->onChange = props.onChange_;
  binding->enabled = props.enabled_;
  bindEnabledState(props.enabled_);
  const int bound = props.value_ ? props.value_->get() : props.min_;
  const int shown = loka::app::ScrollBarClampValue(bound, props.min_, props.max_);
  SetControlValue(binding->control, static_cast<short>(shown));
  binding->appliedValue = shown;

  const bool enabledNow = !props.enabled_ || props.enabled_->get();
  binding->active = enabledNow && loka::app::ScrollBarIsScrollable(props.min_, props.max_);
  // Classic's one presentation for "cannot be used right now", whether the
  // reason is a disabled binding or a range with nowhere to go.
  HiliteControl(binding->control, binding->active ? 0 : 255);
  ShowControl(binding->control);
  return true;
}

void ToolboxScenePlatformController::destroyScrollBarControl(short resourceId,
                                                              loka::app::scene::NativeLifetimeHint lifetimeHint)
{
  for (size_t i = 0; i < scrollBarLedger_.scrollBarControls_.size(); ++i)
  {
    ScrollBarControlBinding &binding = scrollBarLedger_.scrollBarControls_[i];
    if (binding.resourceId != resourceId)
    {
      continue;
    }
    ControlRef control = binding.control;
    loka::core::State<bool> *enabled = binding.enabled;
    binding.control = 0;
    binding.value = 0;
    binding.onChange = 0;
    binding.enabled = 0;
    scrollBarLedger_.scrollBarControls_.erase(scrollBarLedger_.scrollBarControls_.begin() + i);
    controlIds_.release(resourceId);
    // The scroll bar is not hit-list based, so the enabled unbind that
    // releaseNodeContexts performs for buttons and popups happens here
    // instead -- same rule: the observer goes only when no live binding of
    // any kind still needs it.
    if (enabled && !hasLiveBinding(enabled))
    {
      unbindEnabledState(enabled);
    }
    if (control)
    {
      // Context destruction can run inside an update pass; disposal waits for
      // the platform safe point like every other retired native handle.
      HideControl(control);
      queueRetiredScrollBarControl(control, lifetimeHint);
    }
    return;
  }
  // An auto id can exist before NewControl succeeds. The viewport ledger
  // still owns that id and must return it when its node retires.
  controlIds_.release(resourceId);
}

int ToolboxScenePlatformController::ensureViewportScrollBarControl(
    const Rect &viewportRect,
    loka::app::ScrollViewNode *scrollView,
    int contentHeight,
    int viewportHeight,
    int requestedOffset)
{
  const ToolboxScrollViewMetrics metrics = ToolboxScrollViewResolveMetrics(
      contentHeight, viewportHeight, requestedOffset);
  ViewportScrollBarBinding *binding = 0;
  for (std::size_t i = 0; i < this->scrollBarLedger_.viewportScrollBars_.size(); ++i)
  {
    if (this->scrollBarLedger_.viewportScrollBars_[i].scrollView == scrollView)
    {
      binding = &this->scrollBarLedger_.viewportScrollBars_[i];
      break;
    }
  }
  if (!binding)
  {
    ViewportScrollBarBinding entry;
    entry.resourceId = this->allocateControlId();
    entry.scrollView = scrollView;
    entry.usedThisFrame = true;
    entry.rect = viewportRect;
    scrollBarLedger_.viewportScrollBars_.push_back(entry);
    binding = &scrollBarLedger_.viewportScrollBars_.back();
  }
  binding->usedThisFrame = true;
  binding->rect = viewportRect;

  Rect barRect = viewportRect;
  const int proposedLeft =
      static_cast<int>(viewportRect.right) - loka::app::SCROLL_BAR_THICKNESS;
  barRect.left = proposedLeft > viewportRect.left
                     ? static_cast<short>(proposedLeft)
                     : viewportRect.left;
  ScrollBarControlBinding *native = 0;
  if (!this->ensureScrollBarBinding(
          binding->resourceId,
          barRect,
          0,
          metrics.maximum,
          1,
          viewportHeight > 1 ? viewportHeight - 1 : 1,
          scrollView->nativeLifetimeHint(),
          native))
  {
    return metrics.clampedOffset;
  }
  if (native)
  {
    native->value = 0;
    native->onChange = 0;
    native->enabled = 0;
    SetControlValue(native->control,
                    static_cast<short>(metrics.clampedOffset));
    native->appliedValue = metrics.clampedOffset;
    native->active = metrics.maximum > 0;
    HiliteControl(native->control, native->active ? 0 : 255);
    ShowControl(native->control);
  }
  return metrics.clampedOffset;
}

void ToolboxScenePlatformController::destroyViewportScrollBarControl(
    loka::app::ScrollViewNode *scrollView,
    loka::app::scene::NativeLifetimeHint lifetimeHint)
{
  for (std::size_t index = 0; index < this->scrollBarLedger_.viewportScrollBars_.size(); ++index)
  {
    ViewportScrollBarBinding &binding = scrollBarLedger_.viewportScrollBars_[index];
    if (binding.scrollView != scrollView)
    {
      continue;
    }
    const short resourceId = binding.resourceId;
    scrollBarLedger_.viewportScrollBars_.erase(scrollBarLedger_.viewportScrollBars_.begin() + index);
    this->destroyScrollBarControl(resourceId, lifetimeHint);
    return;
  }
}

void ToolboxScenePlatformController::commitScrollBarValueAt(std::size_t index)
{
  if (index >= scrollBarLedger_.scrollBarControls_.size())
  {
    return;
  }
  ScrollBarControlBinding &binding = scrollBarLedger_.scrollBarControls_[index];
  if (!binding.control || !binding.value)
  {
    return;
  }
  const int settled = static_cast<int>(GetControlValue(binding.control));
  if (settled == binding.appliedValue)
  {
    // A cancelled thumb drag lands back on the value the CDEF started from.
    // Publishing it anyway would fire onChange for a gesture the user
    // deliberately abandoned.
    return;
  }
  loka::core::MutableState<int> *mutableValue =
      static_cast<loka::core::MutableState<int> *>(binding.value->asMutableState());
  if (!mutableValue)
  {
    return;
  }
  // Copy out before the batch: publishing re-enters projection, which can
  // reallocate scrollBarControls_ underneath this reference.
  const Rect rect = binding.rect;
  loka::core::EmitterState *onChange = binding.onChange;
  binding.appliedValue = settled;

  beginBatchUpdate();
  addPendingDirty(rect);
  // The write enters the window tracker's transaction the way handleTextKey
  // already does for typing: a settled value is scene input, and dependent
  // state resolves in the same transaction rather than at whatever tick
  // happens next.
  {
    loka::core::StateTrackerGuard _(window_ ? window_->getTracker() : 0);
    // Order is the contract (ruling 1): the binding holds the settled value
    // before any handler runs.
    mutableValue->set(settled, true);
    if (onChange)
    {
      onChange->emit();
    }
  }
  endBatchUpdate();
}

void ToolboxScenePlatformController::commitViewportScrollBarValue(
    ViewportScrollBarBinding &binding,
    ScrollBarControlBinding &native)
{
  const int settled = static_cast<int>(GetControlValue(native.control));
  if (settled == native.appliedValue)
  {
    // Cancelled thumb drags and range-edge presses publish no fact.
    return;
  }
  loka::app::ScrollViewNode *scrollView = binding.scrollView;
  const Rect rect = binding.rect;
  native.appliedValue = settled;

  beginBatchUpdate();
  addPendingDirty(rect);
  // Unlike the DSL ScrollBar commit above, this fact belongs to the
  // ScrollView owner. The complete NodeState door selects that tracker.
  if (scrollView->props.offset_.isValid())
  {
    scrollView->props.offset_.set(settled);
  }
  endBatchUpdate();
}

bool ToolboxScenePlatformController::handleControlClick(const Point &point)
{
  if (!window_ || !window_->window())
  {
    return false;
  }
  ControlRef control = 0;
  ControlPartCode part = FindControl(point, window_->window(), &control);
  if (part == 0 || !control)
  {
    return false;
  }
  for (size_t i = 0; i < buttonControls_.size(); ++i)
  {
    ButtonControlBinding &binding = buttonControls_[i];
    if (binding.control == control && binding.emitter)
    {
      if (binding.enabled && !binding.enabled->get())
      {
        return true;
      }
      beginBatchUpdate();
      ControlPartCode tracked = TrackControl(control, point, 0);
      if (tracked != 0)
      {
        binding.emitter->emit();
      }
      endBatchUpdate();
      return true;
    }
  }
  for (size_t i = 0; i < scrollBarLedger_.scrollBarControls_.size(); ++i)
  {
    if (scrollBarLedger_.scrollBarControls_[i].control != control)
    {
      continue;
    }
    std::size_t viewportIndex = scrollBarLedger_.viewportScrollBars_.size();
    for (size_t j = 0; j < scrollBarLedger_.viewportScrollBars_.size(); ++j)
    {
      if (scrollBarLedger_.viewportScrollBars_[j].resourceId ==
          scrollBarLedger_.scrollBarControls_[i].resourceId)
      {
        viewportIndex = j;
        break;
      }
    }
    if (!scrollBarLedger_.scrollBarControls_[i].active)
    {
      // An inactive bar still owns its rect; swallowing the click keeps the
      // hit from falling through to whatever is drawn beneath it.
      return true;
    }
    gActiveScrollBarLineStep = scrollBarLedger_.scrollBarControls_[i].lineStep;
    gActiveScrollBarPageStep = scrollBarLedger_.scrollBarControls_[i].pageStep;
    // The thumb gets no action proc: the CDEF's own outline drag is the
    // Classic gesture. Arrows and page areas need one so a held press keeps
    // moving instead of stepping once.
    ControlActionUPP action = (part == kControlIndicatorPart) ? 0 : ScrollBarActionUPP();
    TrackControl(control, point, action);
    // Read after the loop has ended, never during it (ruling 1). The return
    // code is deliberately ignored: releasing off an arrow ends the scroll
    // but keeps what already scrolled, and commitScrollBarValueAt is the one
    // place that decides whether anything actually changed.
    if (viewportIndex != scrollBarLedger_.viewportScrollBars_.size())
    {
      commitViewportScrollBarValue(
          scrollBarLedger_.viewportScrollBars_[viewportIndex], scrollBarLedger_.scrollBarControls_[i]);
    }
    else
    {
      commitScrollBarValueAt(i);
    }
    return true;
  }
  return false;
}
