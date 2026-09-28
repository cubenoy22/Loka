/** Included by the native context and the host input fixture. */
bool ToolboxCellContext::handleMouseDown(const Point &point, ToolboxScenePlatformController *controller)
{
  if (!node_ || !node_->props.onClick_)
  {
    return false;
  }
  if (!PtInRect(point, &rect_))
  {
    return false;
  }
  if (controller)
  {
    controller->emitHitEmitter(node_->props.onClick_);
  }
  return true;
}
