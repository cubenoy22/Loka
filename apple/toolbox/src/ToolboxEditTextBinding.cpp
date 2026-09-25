/** Shared production binding path, also compiled by the host TextEdit fixture. */
TEHandle ToolboxScenePlatformController::ensureEditTextControl(ToolboxEditTextContext *ownerContext,
                                                               const Rect &rect,
                                                               loka::core::State<loka::core::String> *text,
                                                               loka::app::scene::NativeLifetimeHint lifetimeHint)
{
  if (!text || !ownerContext)
  {
    return 0;
  }
  Rect controlRect;
  if (!this->intersectWithProjectionClip(rect, controlRect))
  {
    return 0;
  }
  EditTextControlBinding *binding = 0;
  loka::core::State<loka::core::String> *previousText = 0;
  size_t bindingIndex = 0;
  if (editControls_.find(ownerContext, bindingIndex))
  {
    binding = &editControls_[bindingIndex];
    previousText = binding->text;
    binding->text = text;
    binding->textSeat = ownerContext->projectedWriteSeat();
  }
  if (!binding)
  {
    TEHandle te = 0;
    if (textEditBucket_.tryAcquire(te))
    {
      // Restore the fresh-created baseline: pooled records keep their last
      // text and rects, and the new binding starts from lastText == "".
      TESetText(static_cast<const void *>(""), 0, te);
      (**te).destRect = controlRect;
      (**te).viewRect = controlRect;
      TECalText(te);
    }
    else
    {
      te = TENew(&controlRect, &controlRect);
      if (!te)
      {
        return 0;
      }
    }
    EditTextControlBinding entry;
    entry.ownerContext = ownerContext;
    entry.editor = 0;
    entry.text = text;
    entry.textSeat = ownerContext->projectedWriteSeat();
    entry.te = te;
    entry.rect = controlRect;
    entry.usedThisFrame = true;
    entry.lastText = "";
    entry.lifetimeHint = lifetimeHint;
    editControls_.add(entry);
    binding = &editControls_.back();
    syncEditTextFromState(*binding);
    TEAutoView(true, binding->te);
  }
  if (this->fallbackFocusContext() == ownerContext)
    this->fallbackFocus_.cut();
  // A native EditText is a live String projection just like Text and the
  // fallback EditHit. Register it at the same seam so programmatic writes can
  // reach TESetText even when no render walk follows the write.
  bindTextState(text);
  if (previousText && previousText != text && !this->hasLiveBinding(previousText))
  {
    unbindTextState(previousText);
  }
  binding->usedThisFrame = true;
  binding->lifetimeHint = lifetimeHint;
  if (binding->rect.left != controlRect.left || binding->rect.top != controlRect.top || binding->rect.right != controlRect.right
      || binding->rect.bottom != controlRect.bottom)
  {
    binding->rect = controlRect;
    if (binding->te)
    {
      (**binding->te).destRect = controlRect;
      (**binding->te).viewRect = controlRect;
      TECalText(binding->te);
      TEAutoView(true, binding->te);
    }
  }
  if (!inBatchUpdate_)
  {
    syncEditTextFromState(*binding);
  }
  return binding ? binding->te : 0;
}

void ToolboxScenePlatformController::retireEditTextBinding(
    EditTextControlBinding &binding,
    loka::app::scene::NativeLifetimeHint lifetimeHint)
{
  if (binding.editor)
    binding.editor->invalidateNativePresentation();
  else if (binding.ownerContext)
    static_cast<ToolboxEditTextContext *>(binding.ownerContext)->invalidateNativePresentation();
  if (binding.te)
  {
    TEDeactivate(binding.te);
    // The EditText churn pool accepts only ordinary EditText records. Editors
    // capture Monaco 9 and a whole-document layout; dispose at the same clock
    // safe point instead of offering their incompatible recipe to that pool.
    queueRetiredTextEdit(binding.te, binding.editor ? loka::app::scene::NATIVE_HINT_EAGER_RELEASE : lifetimeHint);
    binding.te = 0;
  }
  binding.textSeat = loka::app::scene::WriteSeat<loka::core::String>();
}

void ToolboxScenePlatformController::retireEditTextControlAt(
    std::size_t index,
    loka::app::scene::NativeLifetimeHint lifetimeHint)
{
  EditTextControlBinding &binding = editControls_[index];
  loka::core::State<loka::core::String> *retiredText = binding.text;
  this->retireEditTextBinding(binding, lifetimeHint);
  editControls_.erase(index);
  if (retiredText && !this->hasLiveBinding(retiredText))
  {
    unbindTextState(retiredText);
  }
}

void ToolboxScenePlatformController::retireEditTextControl(
    loka::app::scene::NodeContext *ownerContext,
    loka::app::scene::NativeLifetimeHint lifetimeHint)
{
  std::size_t index = 0;
  if (editControls_.find(ownerContext, index))
  {
    this->retireEditTextControlAt(index, lifetimeHint);
  }
}

void ToolboxScenePlatformController::syncEditTextFromState(EditTextControlBinding &binding)
{
  if (binding.editor) return;
  if (!binding.te)
  {
    return;
  }
  std::string utf8;
  if (binding.text)
    loka::platform::CollectUtf8(binding.text->get(), utf8);
  if (binding.lastText == utf8)
  {
    return;
  }
  TESetText(utf8.c_str(), static_cast<long>(utf8.size()), binding.te);
  TESetSelect(utf8.size(), utf8.size(), binding.te);
  binding.lastText = utf8;
}
