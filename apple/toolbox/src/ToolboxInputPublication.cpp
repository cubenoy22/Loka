/** Included by the production controller and Linux host fixture. */
void ToolboxScenePlatformController::emitHitEmitter(loka::core::EmitterState *emitter)
{
  if (!emitter)
  {
    return;
  }
  beginBatchUpdate();
  emitter->emit();
  endBatchUpdate();
}

void ToolboxScenePlatformController::applyPopupSelectionChange(const Rect &rect,
                                                               loka::app::scene::BoundaryNode *,
                                                               loka::core::State<int> *selectedIndex,
                                                               const loka::app::scene::WriteSeat<int> &selectedIndexSeat,
                                                               loka::core::EmitterState *onChange,
                                                               int newIndex)
{
  if (!selectedIndex)
  {
    return;
  }
  beginBatchUpdate();
  addPendingDirty(rect);
  selectedIndexSeat.set(newIndex, true);
  if (onChange)
  {
    onChange->emit();
  }
  endBatchUpdate();
}

