#ifndef LOKA_WIN32_OPEN_FILE_DIALOG_CONTEXT_HPP
#define LOKA_WIN32_OPEN_FILE_DIALOG_CONTEXT_HPP

#include <windows.h>
#include "app/scene/projection/NativeNodeContext.hpp"
#include "app/core/DialogResultTransport.hpp"

class Win32DialogResultTestAccess;

namespace loka
{
  namespace app
  {
    namespace scene
    {
      class PlatformNodeHandlerRegistry;
    }
  } // namespace app
} // namespace loka

class Win32OpenFileDialogContext : public loka::app::scene::NativeNodeContext
{
public:
  Win32OpenFileDialogContext(HWND parent, loka::app::OpenFileDialogNode *node,
                             Window *window = 0);
  virtual ~Win32OpenFileDialogContext();
  /** Attach-time read (late-subscriber rule): presentation from the current
      fact, called by the installing handler right after setContext. */
  void readLifecycleFactOnAttach();
  virtual void onFactChanged(loka::app::scene::NodeLifecycleFact previous,
                             loka::app::scene::NodeLifecycleFact next);
  void presentIfNeeded();
  virtual void onPropsApplied();
  static UINT deferredResultMessage();
  static bool handlePostedResultMessage(UINT message, WPARAM wParam, LPARAM lParam);

private:
  void applyAttachedPresentation();
  void applyDetachedPresentation();
  void presentDialog();
  static void queueDeferredResult(loka::app::DialogResultTransport::ReturnPort &port,
                                  const loka::app::FileChooserResult &result);
  void detachOwnedDialog();
  friend class Win32DialogResultTestAccess;
  Win32OpenFileDialogContext(const Win32OpenFileDialogContext &);
  Win32OpenFileDialogContext &operator=(const Win32OpenFileDialogContext &);

  HWND parent_;
  loka::app::OpenFileDialogNode *node_;
  loka::app::DialogResultTransport *transport_;
  loka::app::OpenFileDialogPresentationPhase presentation_;
  loka::app::DialogResultTransport::Registration *registration_;
};

void RegisterWin32OpenFileDialogNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry);

#endif // LOKA_WIN32_OPEN_FILE_DIALOG_CONTEXT_HPP
