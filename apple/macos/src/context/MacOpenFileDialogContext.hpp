#ifndef LOKA_MAC_OPEN_FILE_DIALOG_CONTEXT_HPP
#define LOKA_MAC_OPEN_FILE_DIALOG_CONTEXT_HPP

#include "core2/scene/NativeNodeContext.hpp"
#include "app/OpenFileDialog.hpp"

class MacOpenFileDialogContext : public declara::core::scene::NativeNodeContext
{
public:
  MacOpenFileDialogContext(void *parentView, declara::app::OpenFileDialogNode *node);
  virtual ~MacOpenFileDialogContext();

private:
  void bindVisible();
  void unbindVisible();
  void applyVisible();
  void presentDialog();

  static void VisibleChangedThunk(void *userData);

  void *parentView_;
  declara::app::OpenFileDialogNode *node_;
  State<bool> *visibleState_;
  bool lastVisible_;
  bool presenting_;
};

#endif // LOKA_MAC_OPEN_FILE_DIALOG_CONTEXT_HPP
