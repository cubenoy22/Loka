// Included by the controller and the Toolbox host fixture.
void ToolboxScenePlatformController::drawControlsInRect(const Rect &rect)
{
  for (size_t i = 0; i < buttonControls_.size(); ++i)
  {
    ButtonControlBinding &binding = buttonControls_[i];
    if (!binding.control || !binding.usedThisFrame)
    {
      continue;
    }
    if (rect.right < binding.rect.left || rect.left > binding.rect.right || rect.bottom < binding.rect.top
        || rect.top > binding.rect.bottom)
    {
      continue;
    }
    if (binding.context)
      binding.context->repaint(binding.control, binding.label);
    else
      Draw1Control(binding.control);
    ++debugStats_.controlDrawCount;
    ++debugStats_.totalControlDrawCount;
  }
  for (size_t i = 0; i < scrollBarLedger_.scrollBarControls_.size(); ++i)
  {
    ScrollBarControlBinding &binding = scrollBarLedger_.scrollBarControls_[i];
    if (!binding.control || !binding.usedThisFrame)
    {
      continue;
    }
    if (rect.right < binding.rect.left || rect.left > binding.rect.right || rect.bottom < binding.rect.top
        || rect.top > binding.rect.bottom)
    {
      continue;
    }
    Draw1Control(binding.control);
    ++debugStats_.controlDrawCount;
    ++debugStats_.totalControlDrawCount;
  }
}


