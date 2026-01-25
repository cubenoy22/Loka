#ifndef LOKA_TOOLBOX_OPEN_FILE_DIALOG_CONTEXT_HPP
#define LOKA_TOOLBOX_OPEN_FILE_DIALOG_CONTEXT_HPP

#include "core2/scene/NativeNodeContext.hpp"
#include "app/OpenFileDialog.hpp"

class ToolboxOpenFileDialogContext : public loka::core::scene::NativeNodeContext
{
public:
  explicit ToolboxOpenFileDialogContext(loka::app::OpenFileDialogNode *node);
  virtual ~ToolboxOpenFileDialogContext();

private:
  void bindVisible();
  void unbindVisible();
  void applyVisible();
  void presentDialog();
  void setResult(const loka::app::FileChooserResult &result);

  static void VisibleChangedThunk(void *userData);

  loka::app::OpenFileDialogNode *node_;
  State<bool> *visibleState_;
  MutableState<loka::app::FileChooserResult> *resultState_;
  EmitterState *onResult_;
  bool lastVisible_;
  bool presenting_;
};

#endif // LOKA_TOOLBOX_OPEN_FILE_DIALOG_CONTEXT_HPP
