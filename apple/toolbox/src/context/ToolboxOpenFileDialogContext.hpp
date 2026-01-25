#ifndef LOKA_TOOLBOX_OPEN_FILE_DIALOG_CONTEXT_HPP
#define LOKA_TOOLBOX_OPEN_FILE_DIALOG_CONTEXT_HPP

#include "app/scene/NativeNodeContext.hpp"
#include "app/OpenFileDialog.hpp"

class ToolboxOpenFileDialogContext : public loka::app::scene::NativeNodeContext
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
  loka::core::State<bool> *visibleState_;
  loka::core::MutableState<loka::app::FileChooserResult> *resultState_;
  loka::core::EmitterState *onResult_;
  bool lastVisible_;
  bool presenting_;
};

#endif // LOKA_TOOLBOX_OPEN_FILE_DIALOG_CONTEXT_HPP
