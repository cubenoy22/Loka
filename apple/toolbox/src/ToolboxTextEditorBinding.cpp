#include "context/ToolboxLayoutUtil.hpp"

/** Included by the controller and the host fixture: one native installation path. */
TEHandle ToolboxScenePlatformController::ensureTextEditorControl(ToolboxTextEditorContext *context,
                                                                 const Rect &rect,
                                                                 loka::app::scene::NativeLifetimeHint hint)
{
  Rect clipped;
  if (!this->intersectWithProjectionClip(rect, clipped))
    return 0;
  std::size_t index = 0;
  if (this->editControls_.find(context, index))
  {
    EditTextControlBinding &binding = this->editControls_[index];
    binding.usedThisFrame = true;
    binding.rect = clipped;
    (**binding.te).viewRect = clipped;
    return binding.te;
  }
  ToolboxTextMeasureScope fontScope(*this);
  // Plain TextEdit captures the port font and its metrics at creation.
  TextFont(4); // Monaco; Universal Interfaces omit the legacy monaco constant.
  TextSize(9);
  TextFace(0);
  TEHandle te = TENew(&rect, &clipped);
  if (!te)
    return 0;
  EditTextControlBinding entry;
  entry.ownerContext = context;
  entry.editor = context;
  entry.text = 0;
  entry.te = te;
  entry.rect = clipped;
  entry.usedThisFrame = true;
  entry.lifetimeHint = hint;
  this->editControls_.add(entry);
  TEAutoView(true, te);
  return te;
}
