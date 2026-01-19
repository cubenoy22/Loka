#ifndef LOKA_WIN32_OPEN_FILE_DIALOG_CONTEXT_HPP
#define LOKA_WIN32_OPEN_FILE_DIALOG_CONTEXT_HPP

#include <windows.h>
#include "core2/scene/NativeNodeContext.hpp"
#include "app/OpenFileDialog.hpp"

class Win32OpenFileDialogContext : public declara::core::scene::NativeNodeContext
{
public:
  Win32OpenFileDialogContext(HWND parent, declara::app::OpenFileDialogNode *node);
  virtual ~Win32OpenFileDialogContext();

private:
  void bindVisible();
  void unbindVisible();
  void applyVisible();
  void presentDialog();

  static void VisibleChangedThunk(void *userData);

  HWND parent_;
  declara::app::OpenFileDialogNode *node_;
  State<bool> *visibleState_;
  bool lastVisible_;
  bool presenting_;
};

#endif // LOKA_WIN32_OPEN_FILE_DIALOG_CONTEXT_HPP
