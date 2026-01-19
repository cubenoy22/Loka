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
  void setResult(const declara::app::FileChooserResult &result);

  static void VisibleChangedThunk(void *userData);

  HWND parent_;
  declara::app::OpenFileDialogNode *node_;
  State<bool> *visibleState_;
  MutableState<declara::app::FileChooserResult> *resultState_;
  EmitterState *onResult_;
  bool lastVisible_;
  bool presenting_;
};

#endif // LOKA_WIN32_OPEN_FILE_DIALOG_CONTEXT_HPP
