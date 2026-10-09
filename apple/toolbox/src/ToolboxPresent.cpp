#include "ToolboxPendingDialogs.hpp"

/** Outer admission/render completion, shared with the host fixture. */
void ToolboxApp::present(ActivationPhase phase, loka::core::Operation &turn)
{
  turn.settle();
  this->admitAndApplyWindows();
  if (phase != ACTIVATION_FOREGROUND || !group_)
  {
    turn.close();
    this->reclaimWindows();
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
  if (this->hasPendingDialogs())
  {
    // Delivery may remove windows from the group; App reclamation follows this pass.
    const std::vector<AppComponent *> dialogs = this->group_->getComponents();
    for (std::vector<AppComponent *>::const_iterator it = dialogs.begin(); it != dialogs.end(); ++it)
    {
      // A delivery may quit the app; later windows then present nothing.
      if (!this->running_)
        break;
      Window *window = *it ? (*it)->asWindow() : 0;
      if (!window || this->isWindowClosePending(window))
        continue;
      ToolboxWindow *toolboxWindow = window->asToolboxWindow();
      ToolboxScenePlatformController *controller = toolboxWindow ? toolboxWindow->scenePlatformController() : 0;
      if (!controller || controller->borrowPhase().open())
        continue;
      loka::app::OpenFileDialogProps props;
      if (controller->pendingDialogs().take(props))
      {
        const loka::app::FileChooserResult result = RunToolboxFileDialog(props.options_);
        this->cursorOwner_.reconcile();
        DeliverOpenFileDialogResult(props.result_, props.onResult_, result);
      }
    }
  }
  this->reconcileFocus();
  turn.close();
  this->reclaimWindows();
}

/** Allocation-free check before the dialog pass snapshots the group. */
bool ToolboxApp::hasPendingDialogs() const
{
  if (!this->running_ || !this->group_)
    return false;
  const std::vector<AppComponent *> &comps = this->group_->getComponents();
  for (std::vector<AppComponent *>::const_iterator it = comps.begin(); it != comps.end(); ++it)
  {
    Window *window = *it ? (*it)->asWindow() : 0;
    ToolboxWindow *toolboxWindow = window ? window->asToolboxWindow() : 0;
    ToolboxScenePlatformController *controller = toolboxWindow ? toolboxWindow->scenePlatformController() : 0;
    if (controller && !controller->pendingDialogs().empty())
      return true;
  }
  return false;
}
