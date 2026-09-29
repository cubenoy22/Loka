// Included by the scrollbar ledger and the Toolbox host fixture.
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
