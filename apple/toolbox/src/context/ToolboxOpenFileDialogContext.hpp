#ifndef LOKA_TOOLBOX_OPEN_FILE_DIALOG_CONTEXT_HPP
#define LOKA_TOOLBOX_OPEN_FILE_DIALOG_CONTEXT_HPP

#include "core2/scene/NativeNodeContext.hpp"
#include "app/OpenFileDialog.hpp"

class ToolboxOpenFileDialogContext : public declara::core::scene::NativeNodeContext
{
public:
  explicit ToolboxOpenFileDialogContext(declara::app::OpenFileDialogNode *node);
  virtual ~ToolboxOpenFileDialogContext();

private:
  void bindVisible();
  void unbindVisible();
  void applyVisible();
  void presentDialog();

  static void VisibleChangedThunk(void *userData);

  declara::app::OpenFileDialogNode *node_;
  State<bool> *visibleState_;
  bool lastVisible_;
  bool presenting_;
};

#endif // LOKA_TOOLBOX_OPEN_FILE_DIALOG_CONTEXT_HPP
