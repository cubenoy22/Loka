#include "Win32OpenFileDialogTransportTests.hpp"
#include "support/Win32DialogResultTestAccess.hpp"
#include "support/WindowAdmissionTestApp.hpp"
#include "support/TestVerify.hpp"
#include "Win32Window.hpp"
#include "context/Win32OpenFileDialogContext.hpp"
#include <cstdio>

#ifndef LOKA_A3_BASELINE
Win32OpenFileDialogContext *Win32DialogResultTestAccess::create(Win32Window &window,
                                                                loka::app::OpenFileDialogNode *node)
{
  return new Win32OpenFileDialogContext(window.hwnd(), node, &window);
}
void Win32DialogResultTestAccess::queue(Win32OpenFileDialogContext &context, const loka::app::FileChooserResult &result)
{
  context.registration_ = context.transport_->reserve(context.node_->props);
  LOKA_VERIFY(context.registration_ != 0);
  loka::app::DialogResultTransport::ReturnPort port(context.registration_);
  context.queueDeferredResult(port, result);
  std::fprintf(stderr, "A3 capture: sealed, posted pointer-free wake\n");
}
#endif

namespace
{
  void countResult(void *data)
  {
    ++*static_cast<int *>(data);
    std::fprintf(stderr, "A3 result write/emitter observed\n");
  }
  void dispatchWake(HWND window)
  {
    MSG message;
    const UINT wake = Win32OpenFileDialogContext::deferredResultMessage();
    LOKA_VERIFY(PeekMessage(&message, window, wake, wake, PM_REMOVE));
#ifndef LOKA_A3_BASELINE
    LOKA_VERIFY(message.wParam == 0 && message.lParam == 0);
#endif
    std::fprintf(stderr, "A3 dispatch: production Window WndProc\n");
    DispatchMessage(&message);
  }
  void retiredBeforeWake(bool reclaimOwner)
  {
    using namespace loka::app;
    Win32Window window(0, WindowProps());
    WindowAdmissionTestApp app(window);
    app.flush();
    assert(window.hwnd() != 0);
    loka::core::MutableState<FileChooserResult> *result = new loka::core::MutableState<FileChooserResult>();
    loka::core::PushStateTracker *tracker = new loka::core::PushStateTracker();
    tracker->addState(result);
    loka::core::EmitterState emitter;
    int writes = 0, emits = 0;
    result->bind(&countResult, &writes, false);
    emitter.bind(&countResult, &emits, false);
    OpenFileDialogNode *node = new OpenFileDialogNode(
        OpenFileDialogProps().result(scene::NodeState<FileChooserResult>(result, tracker)).onResult(&emitter));
    Win32OpenFileDialogContext *context = Win32DialogResultTestAccess::create(window, node);
    node->setContext(context);
    Win32DialogResultTestAccess::queue(*context, FileChooserResult::Error(665));
    std::fprintf(stderr, "A3 retire: terminal node deletion, context destructor follows\n");
    delete node;
    if (reclaimOwner)
    {
      result->unbind(&countResult, &writes);
      delete tracker;
      delete result;
      tracker = 0;
      result = 0;
      std::fprintf(stderr, "A3 owner: result and tracker reclaimed\n");
    }
    dispatchWake(window.hwnd());
    LOKA_VERIFY(writes == 0 && emits == 0);
    app.flush();
    app.flush();
    LOKA_VERIFY(writes == 0 && emits == 0);
    if (result)
      result->unbind(&countResult, &writes);
    delete tracker;
    delete result;
    emitter.unbind(&countResult, &emits);
  }
} // namespace

void testWin32OpenFileDialogRetiredBeforePostedWake()
{
  retiredBeforeWake(false);
}
void testWin32OpenFileDialogOwnerReclaimedBeforePostedWake()
{
  retiredBeforeWake(true);
}

void testWin32OpenFileDialogLiveAdmissionAndLostHwnd()
{
#ifndef LOKA_A3_BASELINE
  using namespace loka::app;
  Win32Window window(0, WindowProps());
  WindowAdmissionTestApp app(window);
  app.flush();
  loka::core::EmitterState emitter;
  int emits = 0;
  emitter.bind(&countResult, &emits, false);
  OpenFileDialogNode *node = new OpenFileDialogNode(OpenFileDialogProps().onResult(&emitter));
  Win32OpenFileDialogContext *context = Win32DialogResultTestAccess::create(window, node);
  node->setContext(context);
  Win32DialogResultTestAccess::queue(*context, FileChooserResult::Canceled());
  dispatchWake(window.hwnd());
  LOKA_VERIFY(emits == 0);
  app.flush();
  LOKA_VERIFY(emits == 1);
  delete node;
  app.flush();

  node = new OpenFileDialogNode(OpenFileDialogProps().onResult(&emitter));
  context = Win32DialogResultTestAccess::create(window, node);
  node->setContext(context);
  Win32DialogResultTestAccess::queue(*context, FileChooserResult::Canceled());
  LOKA_VERIFY(DestroyWindow(window.hwnd()));
  delete node;
  // No wake dispatch is needed to drop the old Window-owned entry.
  app.flush();
  app.flush();
  LOKA_VERIFY(emits == 1);
  emitter.unbind(&countResult, &emits);
#endif
}

void testWin32OpenFileDialogMissingHwndUsesAdmission()
{
#ifndef LOKA_A3_BASELINE
  using namespace loka::app;
  Win32Window window(0, WindowProps());
  WindowAdmissionTestApp app(window);
  // Explicit fixture enrollment models a live common owner before rail creation.
  window.dialogResults().open(window);
  assert(!window.hwnd());
  loka::core::MutableState<FileChooserResult> result;
  loka::core::PushStateTracker tracker;
  tracker.addState(&result);
  loka::core::EmitterState emitter;
  int emits = 0;
  emitter.bind(&countResult, &emits, false);
  OpenFileDialogNode *node = new OpenFileDialogNode(OpenFileDialogProps()
      .result(scene::NodeState<FileChooserResult>(&result, &tracker)).onResult(&emitter));
  Win32OpenFileDialogContext *context = Win32DialogResultTestAccess::create(window, node);
  node->setContext(context);
  {
    loka::core::StateTrackerGuard guard(&tracker);
    Win32DialogResultTestAccess::queue(*context, FileChooserResult::Canceled());
    LOKA_VERIFY(emits == 0);
  }
  LOKA_VERIFY(emits == 0 && result.get().kind == FileChooserResult::RESULT_NONE);
  app.flush();
  LOKA_VERIFY(emits == 1 && result.get().kind == FileChooserResult::RESULT_CANCELED);
  delete node;
  app.flush();
  emitter.unbind(&countResult, &emits);
#endif
}
