/** Outer admission/render completion, shared with the host fixture. */
void ToolboxApp::present(ActivationPhase phase)
{
  this->flushWindowInvalidations();
  if (phase != ACTIVATION_FOREGROUND || !group_)
  {
    return;
  }
  const std::vector<AppComponent *> &comps = group_->getComponents();
  for (std::vector<AppComponent *>::const_iterator it = comps.begin(); it != comps.end(); ++it)
  {
    Window *w = (*it)->asWindow();
    ToolboxWindow *toolboxWindow = w ? w->asToolboxWindow() : 0;
    if (toolboxWindow)
    {
      toolboxWindow->flushInvalidate();
    }
  }
  this->reconcileFocus();
}
