#include "Win32OpenFileDialogTransportTests.hpp"
#include "support/Win32DialogResultTestAccess.hpp"
#include "support/WindowAdmissionTestApp.hpp"
#include "support/TestVerify.hpp"
#include "Win32Window.hpp"
#include "context/Win32OpenFileDialogContext.hpp"
#include <cstdio>
#include <cwchar>
#include <string>
#include "platform/Win32String.hpp"

#ifndef LOKA_A3_BASELINE
Win32OpenFileDialogContext *Win32DialogResultTestAccess::create(Win32Window &window,
                                                                loka::app::OpenFileDialogNode *node)
{
  return new Win32OpenFileDialogContext(window.hwnd(), node, &window);
}
bool Win32DialogResultTestAccess::configureSave(const loka::app::FileDialogOptions &options,
                                               wchar_t (&buffer)[MAX_PATH], OPENFILENAMEW &dialog)
{
  return Win32OpenFileDialogContext::configureSaveDialog(options, buffer, dialog);
}
void Win32DialogResultTestAccess::present(Win32OpenFileDialogContext &context)
{
  context.registration_ = context.transport_->reserve(context.node_->props);
  LOKA_VERIFY(context.registration_ != 0);
  context.presentDialog();
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

void testWin32SaveFileDialogInvalidNameUsesAdmission()
{
#ifndef LOKA_A3_BASELINE
  using namespace loka::app;
  Win32Window window(0, WindowProps());
  WindowAdmissionTestApp app(window);
  app.flush();
  loka::core::MutableState<FileChooserResult> storage;
  loka::core::PushStateTracker tracker;
  tracker.addState(&storage);
  scene::NodeState<FileChooserResult> state(&storage, &tracker);
  int writes = 0;
  storage.bind(&countResult, &writes, false);
  OpenFileDialogDefinition definition = SaveFileDialog(loka::core::String::Literal("folder/Untitled")).result(state);
  OpenFileDialogNode node(definition.props);
  node.setContext(Win32DialogResultTestAccess::create(window, &node));
  Win32DialogResultTestAccess::present(*static_cast<Win32OpenFileDialogContext *>(node.getContext()));
  LOKA_VERIFY(writes == 0);
  app.flush();
  LOKA_VERIFY(writes == 1 && storage.get().kind == FileChooserResult::RESULT_ERROR);
  LOKA_VERIFY(storage.get().errorCode == FNERR_INVALIDFILENAME);
  app.flush();
  LOKA_VERIFY(writes == 1);
  storage.unbind(&countResult, &writes);
  tracker.removeState(&storage);
#endif
}

void testWin32SaveFileDialogDefaultSetup()
{
#ifndef LOKA_A3_BASELINE
  using namespace loka::app;
  wchar_t buffer[MAX_PATH];
  OPENFILENAMEW dialog;
  ZeroMemory(&dialog, sizeof(dialog));
  const wchar_t name[] = L"\x65E5\x672C\x8A9E.txt";
  const FileDialogOptions options(FILE_DIALOG_SAVE,
      loka::core::String::Literal("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E.txt"));
  LOKA_VERIFY(Win32DialogResultTestAccess::configureSave(options, buffer, dialog));
  LOKA_VERIFY(std::wcscmp(buffer, name) == 0);
  LOKA_VERIFY(dialog.lpstrFile == buffer && dialog.nMaxFile == MAX_PATH);
  LOKA_VERIFY(dialog.Flags == (OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR));
  LOKA_VERIFY((dialog.Flags & OFN_FILEMUSTEXIST) == 0);
  const wchar_t filter[] = L"All Files\0*.*\0";
  LOKA_VERIFY(std::wmemcmp(dialog.lpstrFilter, filter, sizeof(filter) / sizeof(filter[0])) == 0);
  LOKA_VERIFY(dialog.lpstrDefExt == 0 && dialog.nFilterIndex == 1);
  for (std::size_t i = std::wcslen(name); i < MAX_PATH; ++i)
    LOKA_VERIFY(buffer[i] == L'\0');
#endif
}

void testWin32SaveFileDialogTextSetup()
{
#ifndef LOKA_A3_BASELINE
  using namespace loka::app;
  wchar_t buffer[MAX_PATH];
  OPENFILENAMEW dialog;
  ZeroMemory(&dialog, sizeof(dialog));
  const FileDialogOptions options(FILE_DIALOG_SAVE, loka::core::String::Literal("Untitled"),
                                  FILE_DIALOG_FILTER_ALL_FILES_TEXT);
  LOKA_VERIFY(Win32DialogResultTestAccess::configureSave(options, buffer, dialog));
  const wchar_t filter[] = L"Text\0*.txt\0All Files\0*.*\0";
  LOKA_VERIFY(std::wmemcmp(dialog.lpstrFilter, filter, sizeof(filter) / sizeof(filter[0])) == 0);
  LOKA_VERIFY(std::wcscmp(dialog.lpstrDefExt, L"txt") == 0 && dialog.nFilterIndex == 1);
  LOKA_VERIFY(std::wcscmp(buffer, L"Untitled") == 0);
#endif
}

void testWin32SaveFileDialogNameBounds()
{
#ifndef LOKA_A3_BASELINE
  using namespace loka::app;
  wchar_t buffer[MAX_PATH];
  OPENFILENAMEW dialog;
  ZeroMemory(&dialog, sizeof(dialog));
  const std::wstring fitting(MAX_PATH - 1, L'x');
  LOKA_VERIFY(Win32DialogResultTestAccess::configureSave(FileDialogOptions(FILE_DIALOG_SAVE,
      loka::core::String(loka::win32::CreateWin32StringFromUtf16(fitting.c_str(), fitting.size()))),
      buffer, dialog));
  LOKA_VERIFY(std::wcslen(buffer) == MAX_PATH - 1 && buffer[MAX_PATH - 1] == L'\0');
  const std::wstring oversized(MAX_PATH, L'x');
  LOKA_VERIFY(!Win32DialogResultTestAccess::configureSave(FileDialogOptions(FILE_DIALOG_SAVE,
      loka::core::String(loka::win32::CreateWin32StringFromUtf16(oversized.c_str(), oversized.size()))),
      buffer, dialog));
  LOKA_VERIFY(buffer[0] == L'\0');
  LOKA_VERIFY(Win32DialogResultTestAccess::configureSave(FileDialogOptions(FILE_DIALOG_SAVE), buffer, dialog));
  LOKA_VERIFY(buffer[0] == L'\0');
#endif
}

void testWin32SaveFileDialogInvalidBasenames()
{
#ifndef LOKA_A3_BASELINE
  using namespace loka::app;
  wchar_t buffer[MAX_PATH];
  OPENFILENAMEW dialog;
  ZeroMemory(&dialog, sizeof(dialog));
  const wchar_t invalidUnits[] = { L'\0', L'\x1F', L'<', L'>', L':', L'"', L'/', L'\\', L'|', L'?', L'*' };
  for (std::size_t i = 0; i < sizeof(invalidUnits) / sizeof(invalidUnits[0]); ++i)
  {
    const wchar_t name[] = { L'a', invalidUnits[i], L'b' };
    LOKA_VERIFY(!Win32DialogResultTestAccess::configureSave(FileDialogOptions(FILE_DIALOG_SAVE,
        loka::core::String(loka::win32::CreateWin32StringFromUtf16(name, sizeof(name) / sizeof(name[0])))),
        buffer, dialog));
    LOKA_VERIFY(buffer[0] == L'\0');
  }
  LOKA_VERIFY(!Win32DialogResultTestAccess::configureSave(FileDialogOptions(FILE_DIALOG_SAVE,
      loka::core::String::Literal("name.")), buffer, dialog));
  LOKA_VERIFY(!Win32DialogResultTestAccess::configureSave(FileDialogOptions(FILE_DIALOG_SAVE,
      loka::core::String::Literal("name ")), buffer, dialog));
#endif
}
