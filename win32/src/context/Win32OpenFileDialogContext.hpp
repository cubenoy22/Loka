#ifndef LOKA_WIN32_OPEN_FILE_DIALOG_CONTEXT_HPP
#define LOKA_WIN32_OPEN_FILE_DIALOG_CONTEXT_HPP

#include <windows.h>
#include "app/scene/NativeNodeContext.hpp"
#include "app/OpenFileDialog.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      class PlatformNodeHandlerRegistry;
    }
  }
}

class Win32OpenFileDialogContext : public loka::app::scene::NativeNodeContext
{
public:
  Win32OpenFileDialogContext(HWND parent, loka::app::OpenFileDialogNode *node);
  virtual ~Win32OpenFileDialogContext();
  virtual void onNodeAttached();
  virtual void onNodeDetached();
  void presentIfNeeded();
  static UINT deferredResultMessage();
  static bool handlePostedResultMessage(UINT message, WPARAM wParam, LPARAM lParam);

private:
  struct DeferredResultDelivery
  {
    loka::core::MutableState<loka::app::FileChooserResult> *resultState;
    loka::core::EmitterState *onResult;
    loka::app::FileChooserResult result;
  };

  void presentDialog();
  void setResult(const loka::app::FileChooserResult &result);
  void queueDeferredResult(const loka::app::FileChooserResult &result);

  static void DeliverDeferredResultThunk(void *userData);

  HWND parent_;
  loka::app::OpenFileDialogNode *node_;
  loka::core::MutableState<loka::app::FileChooserResult> *resultState_;
  loka::core::EmitterState *onResult_;
  loka::app::OpenFileDialogPresentationPhase presentation_;
};

void RegisterWin32OpenFileDialogNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry);

#endif // LOKA_WIN32_OPEN_FILE_DIALOG_CONTEXT_HPP
