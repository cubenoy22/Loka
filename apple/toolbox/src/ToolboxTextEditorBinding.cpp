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

/** Shared EditText/TextEditor focus path; native activation follows the focused row. */
bool ToolboxScenePlatformController::handleEditClick(const Point &point)
{
  EditTextControlBinding *focusedEdit = editControls_.focused();
  if (focusedEdit && focusedEdit->te)
  {
    TEDeactivate(focusedEdit->te);
    editControls_.clearFocus();
  }
  for (size_t i = 0; i < editControls_.size(); ++i)
  {
    EditTextControlBinding &binding = editControls_[i];
    if (binding.te && PtInRect(point, &binding.rect))
    {
      editControls_.focus(i);
      this->fallbackFocus_.cut();
      TEActivate(binding.te);
      if (binding.editor) binding.editor->click(point);
      else TEClick(point, false, binding.te);
      return true;
    }
  }
  return false;
}

void ToolboxScenePlatformController::idleTextEdits()
{
  for (size_t i = 0; i < editControls_.size(); ++i)
  {
    if (editControls_[i].editor) editControls_[i].editor->retryProjection();
    if (&editControls_[i] == editControls_.focused() && editControls_[i].te)
    {
      TEIdle(editControls_[i].te);
    }
  }
}
