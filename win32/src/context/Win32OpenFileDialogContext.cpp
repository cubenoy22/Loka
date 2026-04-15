#include "Win32OpenFileDialogContext.hpp"
#include "../Win32ScenePlatformController.hpp"
#include "app/scene/PlatformNodeHandler.hpp"
#include <commdlg.h>

namespace
{
  const UINT kWin32OpenFileDialogDeferredResultMessage = WM_APP + 41;

  static void DeliverOpenFileDialogResult(loka::core::MutableState<loka::app::FileChooserResult> *resultState,
                                          loka::core::EmitterState *onResult,
                                          const loka::app::FileChooserResult &result)
  {
    void *onResultToken = onResult ? onResult->retainExternalLifetimeToken() : 0;
    if (resultState)
    {
      resultState->set(result, true);
    }
    if (onResult && loka::core::StateBase::isExternalLifetimeTokenAlive(onResultToken))
    {
      onResult->emit();
    }
    if (onResultToken)
    {
      loka::core::StateBase::releaseExternalLifetimeToken(onResultToken);
    }
  }

  class Win32OpenFileDialogNodeHandler : public loka::app::scene::IPlatformNodeHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<loka::app::OpenFileDialogNode>();
    }

    virtual loka::app::scene::NodeContext *ensureContext(loka::app::scene::Node *node,
                                                         loka::app::scene::IPlatformController *controller,
                                                         const loka::app::scene::LayoutState &state)
    {
      (void)state;
      loka::app::OpenFileDialogNode *dialog = node ? node->asOpenFileDialogNode() : 0;
      Win32ScenePlatformController *win32 = static_cast<Win32ScenePlatformController *>(controller);
      if (!dialog || !win32)
      {
        return 0;
      }
      return win32->contextMapper()->ensureOpenFileDialogContext(dialog);
    }
  };

  Win32OpenFileDialogNodeHandler gWin32OpenFileDialogNodeHandler;
}

UINT Win32OpenFileDialogContext::deferredResultMessage()
{
  return kWin32OpenFileDialogDeferredResultMessage;
}

bool Win32OpenFileDialogContext::handlePostedResultMessage(UINT message, WPARAM, LPARAM lParam)
{
  if (message != kWin32OpenFileDialogDeferredResultMessage)
  {
    return false;
  }
  DeliverDeferredResultThunk(reinterpret_cast<void *>(lParam));
  return true;
}

Win32OpenFileDialogContext::Win32OpenFileDialogContext(HWND parent, loka::app::OpenFileDialogNode *node)
    : parent_(parent),
      node_(node),
      resultState_(0),
      onResult_(0),
      presentation_()
{
  resultState_ = node_ ? node_->props.result_ : 0;
  onResult_ = node_ ? node_->props.onResult_ : 0;
}

Win32OpenFileDialogContext::~Win32OpenFileDialogContext()
{
}

void Win32OpenFileDialogContext::onNodeAttached()
{
  presentIfNeeded();
}

void Win32OpenFileDialogContext::onNodeDetached()
{
  presentation_.markDetached();
}

void Win32OpenFileDialogContext::presentIfNeeded()
{
  if (!presentation_.beginPresent())
  {
    return;
  }
  presentDialog();
}

void Win32OpenFileDialogContext::presentDialog()
{
  if (!presentation_.isPresenting())
  {
    return;
  }

  char buffer[MAX_PATH];
  buffer[0] = '\0';

  OPENFILENAMEA ofn;
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = parent_;
  ofn.lpstrFile = buffer;
  ofn.nMaxFile = MAX_PATH;
  ofn.lpstrFilter = "Images\0*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tif;*.tiff\0All Files\0*.*\0";
  ofn.nFilterIndex = 1;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

  if (GetOpenFileNameA(&ofn))
  {
    loka::file::File file = loka::file::File::FromPath(loka::core::String(buffer));
    file.setKind(loka::file::File::KIND_FILE);
    setResult(loka::app::FileChooserResult::File(file));
  }
  else
  {
    DWORD error = CommDlgExtendedError();
    if (error != 0)
    {
      setResult(loka::app::FileChooserResult::Error(static_cast<int>(error)));
    }
    else
    {
      setResult(loka::app::FileChooserResult::Canceled());
    }
  }

  presentation_.markPresented();
}

void Win32OpenFileDialogContext::setResult(const loka::app::FileChooserResult &result)
{
  this->queueDeferredResult(result);
}

void Win32OpenFileDialogContext::queueDeferredResult(const loka::app::FileChooserResult &result)
{
  DeferredResultDelivery *delivery = new DeferredResultDelivery();
  delivery->resultState = resultState_;
  delivery->onResult = onResult_;
  delivery->result = result;

  if (parent_ && IsWindow(parent_))
  {
    if (PostMessage(parent_, kWin32OpenFileDialogDeferredResultMessage, 0, reinterpret_cast<LPARAM>(delivery)))
    {
      return;
    }
  }

  loka::core::PushStateTracker *tracker = 0;
  if (delivery->resultState && delivery->resultState->trackerOwner())
  {
    tracker = delivery->resultState->trackerOwner()->asPushTracker();
  }
  if (!tracker && delivery->onResult && delivery->onResult->trackerOwner())
  {
    tracker = delivery->onResult->trackerOwner()->asPushTracker();
  }

  if (!tracker)
  {
    DeliverDeferredResultThunk(delivery);
    return;
  }

  if (tracker->phase() == loka::core::TRACKER_IDLE)
  {
    tracker->begin();
    tracker->defer(&Win32OpenFileDialogContext::DeliverDeferredResultThunk, delivery);
    tracker->end();
    return;
  }

  tracker->defer(&Win32OpenFileDialogContext::DeliverDeferredResultThunk, delivery);
}

void Win32OpenFileDialogContext::DeliverDeferredResultThunk(void *userData)
{
  DeferredResultDelivery *delivery = static_cast<DeferredResultDelivery *>(userData);
  if (!delivery)
  {
    return;
  }
  DeliverOpenFileDialogResult(delivery->resultState, delivery->onResult, delivery->result);

  delete delivery;
}

void RegisterWin32OpenFileDialogNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry)
{
  registry.registerHandler(&gWin32OpenFileDialogNodeHandler);
}
