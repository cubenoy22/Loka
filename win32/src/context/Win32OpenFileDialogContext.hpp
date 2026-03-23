#ifndef LOKA_WIN32_OPEN_FILE_DIALOG_CONTEXT_HPP
#define LOKA_WIN32_OPEN_FILE_DIALOG_CONTEXT_HPP

#include <windows.h>
#include "app/scene/NativeNodeContext.hpp"
#include "app/OpenFileDialog.hpp"

class Win32OpenFileDialogContext : public loka::app::scene::NativeNodeContext
{
public:
  Win32OpenFileDialogContext(HWND parent, loka::app::OpenFileDialogNode *node);
  virtual ~Win32OpenFileDialogContext();

private:
  struct DeferredResultDelivery
  {
    loka::core::MutableState<loka::app::FileChooserResult> *resultState;
    loka::core::EmitterState *onResult;
    loka::app::FileChooserResult result;
  };

  void bindVisible();
  void unbindVisible();
  void applyVisible();
  void presentDialog();
  void setResult(const loka::app::FileChooserResult &result);
  void queueDeferredResult(const loka::app::FileChooserResult &result);

  static void VisibleChangedThunk(void *userData);
  static void DeliverDeferredResultThunk(void *userData);

  HWND parent_;
  loka::app::OpenFileDialogNode *node_;
  loka::core::MutableState<bool> *visibleState_;
  loka::core::MutableState<loka::app::FileChooserResult> *resultState_;
  loka::core::EmitterState *onResult_;
  bool presenting_;
};

#endif // LOKA_WIN32_OPEN_FILE_DIALOG_CONTEXT_HPP
