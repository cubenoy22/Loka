#include "Win32EditTextBridgeTests.hpp"
#include "support/TestVerify.hpp"
#include "support/LokaAllocFailure.hpp"
#include "context/Win32TextEditorContext.hpp"
#include "context/Win32EditTextBridge.hpp"
#include "app/nodes/controls/TextEditorDiff.hpp"
#include "Win32ScenePlatformController.hpp"
#include "Win32BuiltInSupport.hpp"
#include "platform/StringUTF8.hpp"
#include <cstdio>
#include <vector>

namespace loka
{
  namespace testing
  {
    class Win32TextEditorAccess
    {
    public:
      static unsigned restores(const Win32TextEditorContext &context)
      {
        return context.restores_;
      }
      static app::EditorResult status(const Win32TextEditorContext &context)
      {
        return context.status_;
      }
      static WNDPROC nativeProcedure(const Win32TextEditorContext &context)
      {
        return context.previousProc_;
      }
      static bool pending(const Win32TextEditorContext &context)
      {
        return context.phase_ == Win32TextEditorContext::RETRY;
      }
    };
  } // namespace testing
} // namespace loka
namespace
{
  using namespace loka::app;
  using namespace loka::app::scene;
  using namespace loka::core;
  typedef loka::testing::Win32TextEditorAccess EditorAccess;
  const wchar_t kHostController[] = L"Loka.Editor.TestHost";
  const wchar_t kNotification[] = L"Loka.Editor.TestNotification";
  const wchar_t kProbe[] = L"Loka.Editor.TestProbe";

  const wchar_t kChangeObservation[] = L"Loka.Editor.TestChangeObservation";
  /** Stack-scoped observation of native text before the controller can restore it. */
  class ChangeObservation
  {
  public:
    explicit ChangeObservation(HWND host)
        : host_(host),
          arrivals_(0),
          native_(),
          status_(EDITOR_OK)
    {
      LOKA_VERIFY(SetPropW(this->host_, kChangeObservation, this));
    }
    ~ChangeObservation()
    {
      RemovePropW(this->host_, kChangeObservation);
    }
    void arriving(HWND editor)
    {
      ++this->arrivals_;
      const int length = GetWindowTextLengthW(editor);
      std::vector<wchar_t> text(static_cast<std::size_t>(length) + 1);
      const int copied = GetWindowTextW(editor, &text[0], length + 1);
      this->native_.assign(&text[0], static_cast<std::size_t>(copied));
    }
    void handled(HWND editor)
    {
      Win32TextEditorContext *context = Win32TextEditorContext::fromWindow(editor);
      LOKA_VERIFY(context);
      this->status_ = EditorAccess::status(*context);
    }
    unsigned arrivals() const
    {
      return this->arrivals_;
    }
    const std::wstring &native() const
    {
      return this->native_;
    }
    EditorResult status() const
    {
      return this->status_;
    }

  private:
    ChangeObservation(const ChangeObservation &);
    ChangeObservation &operator=(const ChangeObservation &);
    HWND host_;
    unsigned arrivals_;
    std::wstring native_;
    EditorResult status_;
  };

  LRESULT CALLBACK HostProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
  {
    Win32ScenePlatformController *controller =
        static_cast<Win32ScenePlatformController *>(GetPropW(window, kHostController));
    if (message == WM_COMMAND && controller)
    {
      HANDLE previousNotification = GetPropW(window, kNotification);
      LOKA_VERIFY(SetPropW(window, kNotification, controller));
      ChangeObservation *observation =
          HIWORD(wParam) == EN_CHANGE ? static_cast<ChangeObservation *>(GetPropW(window, kChangeObservation)) : 0;
      if (observation)
        observation->arriving(reinterpret_cast<HWND>(lParam));
      const bool handled = controller->handleCommand(wParam, lParam);
      if (observation)
        observation->handled(reinterpret_cast<HWND>(lParam));
      if (previousNotification)
        LOKA_VERIFY(SetPropW(window, kNotification, previousNotification));
      else
        RemovePropW(window, kNotification);
      if (handled)
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
  }
  HWND createHost()
  {
    WNDCLASSW nativeClass = {0};
    nativeClass.lpfnWndProc = &HostProc;
    nativeClass.hInstance = GetModuleHandleW(NULL);
    nativeClass.lpszClassName = L"LokaTextEditorTestHost";
    LOKA_VERIFY(RegisterClassW(&nativeClass) || GetLastError() == ERROR_CLASS_ALREADY_EXISTS);
    HWND window = CreateWindowExW(0,
                                  nativeClass.lpszClassName,
                                  L"editor-host",
                                  WS_OVERLAPPED,
                                  0,
                                  0,
                                  640,
                                  480,
                                  NULL,
                                  NULL,
                                  nativeClass.hInstance,
                                  NULL);
    LOKA_VERIFY(window);
    return window;
  }
  /** Native message instrumentation; setter failures live in TestingHooks.cpp. */
  struct Probe
  {
    HWND window;
    WNDPROC previous;
    unsigned lineReads, textReads, sets;
    bool echo, restoredInsideNotification, seedUndo;
    explicit Probe(HWND value)
        : window(value),
          previous(0),
          lineReads(0),
          textReads(0),
          sets(0),
          echo(false),
          restoredInsideNotification(false),
          seedUndo(false)
    {
      LOKA_VERIFY(SetPropW(window, kProbe, this));
      previous = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&proc)));
      LOKA_VERIFY(previous);
    }
    ~Probe()
    {
      // A retired context has already removed its route and restored EDIT's
      // original procedure. Do not reinstall that retired route here.
      if (IsWindow(window) && Win32TextEditorContext::fromWindow(window))
        SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(previous));
      if (IsWindow(window))
        RemovePropW(window, kProbe);
    }
    static LRESULT CALLBACK proc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
      Probe &probe = *static_cast<Probe *>(GetPropW(window, kProbe));
      if (message == EM_GETLINE)
        ++probe.lineReads;
      if (message == WM_GETTEXT)
        ++probe.textReads;
      if (message == WM_SETTEXT)
      {
        ++probe.sets;
        if (GetPropW(GetParent(window), kNotification))
          probe.restoredInsideNotification = true;
        if (probe.echo)
          SendMessageW(GetParent(window), WM_COMMAND, MAKEWPARAM(0, EN_CHANGE), reinterpret_cast<LPARAM>(window));
      }
      const LRESULT result = CallWindowProcW(probe.previous, window, message, wParam, lParam);
      if (message == WM_SETTEXT && result && probe.seedUndo)
      {
        probe.seedUndo = false;
        // WM_SETTEXT normally resets EDIT undo itself. Seed a real undo record
        // with unchanged final text to discriminate the explicit empty-undo
        // door independently of that incidental setter behavior. Bypass only
        // the input wrapper, as a native replacement implementation would.
        Win32TextEditorContext *context = Win32TextEditorContext::fromWindow(window);
        LOKA_VERIFY(context);
        WNDPROC native = EditorAccess::nativeProcedure(*context);
        CallWindowProcW(native, window, EM_SETSEL, 0, 0);
        CallWindowProcW(native, window, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(L"x"));
        CallWindowProcW(native, window, EM_SETSEL, 0, 1);
        CallWindowProcW(native, window, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(L""));
        LOKA_VERIFY(SendMessageW(window, EM_CANUNDO, 0, 0));
      }
      return result;
    }
  };
  class PlainOnlyHighlighter : public LineHighlighter
  {
  public:
    mutable unsigned calls;
    PlainOnlyHighlighter()
        : calls(0)
    {
    }
    virtual bool highlight(const String &line, AttributedString::Builder &out) const
    {
      ++this->calls;
      return out.append(line, Bold);
    }
  };
  struct Fixture
  {
    HWND host;
    Win32ScenePlatformController controller;
    PushStateTracker tracker;
    ObservableList<String> lines;
    MutableState<LineCursor> cursor;
    NodeState<LineCursor> seat;
    TextEditorNode *node;
    Win32TextEditorContext *context;
    Fixture(unsigned short count = 3, const std::string &text = "abcd", unsigned short capacity = 256)
        : host(createHost()),
          controller(host, loka::win32::Win32DisplayScale(96, RailMetrics())),
          tracker(),
          lines(),
          cursor(),
          seat(&cursor, &tracker),
          node(0),
          context(0)
    {
      LOKA_VERIFY(SetPropW(host, kHostController, &controller));
      RegisterWin32BuiltInSupport(controller);
      tracker.addState(&cursor);
      LOKA_VERIFY(lines.attach(&tracker, capacity) == ATTACH_OK);
      for (unsigned short i = 0; i < count; ++i)
        LOKA_VERIFY(lines.insert(i, String(text)) == EDIT_OK);
      if (count)
      {
        StateTrackerGuard guard(&tracker);
        cursor.set(LineCursor(lines.at(0).id, text.size() < 2 ? static_cast<int>(text.size()) : 2));
      }
      node = new TextEditorNode(TextEditorProps(lines, seat));
      LayoutState bounds;
      bounds.x = 10;
      bounds.y = 20;
      bounds.width = 300;
      bounds.height = 120;
      bounds.spacing = 4;
      LOKA_VERIFY(controller.prepareProjectedLayout(node, bounds));
      context = static_cast<Win32TextEditorContext *>(node->getContext());
      LOKA_VERIFY(context && context->hwnd() && IsWindowUnicode(context->hwnd()));
    }
    ~Fixture()
    {
      loka::win32::testing::failTextEditorSets(loka::win32::testing::TEXT_EDITOR_SET_REFUSED, 0);
      delete node;
      controller.drainNativeRetirements();
      RemovePropW(host, kHostController);
      LOKA_VERIFY(DestroyWindow(host));
    }
    void type(wchar_t value)
    {
      if (value == L'\b')
      {
        SendMessageW(context->hwnd(), WM_CHAR, value, 1);
        return;
      }
      DWORD start = 0, end = 0;
      SendMessageW(context->hwnd(), EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
      SendMessageW(context->hwnd(), EM_SETSEL, start, end);
      const wchar_t text[] = {value, 0};
      // EDIT sends EN_CHANGE to HostProc for replacement; multiline WM_SETTEXT
      // is a projection operation and deliberately sends no input notification.
      SendMessageW(context->hwnd(), EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(value == L'\r' ? L"\r\n" : text));
    }
    std::wstring native() const
    {
      std::wstring result;
      loka::win32::ReadEditTextWide(context->hwnd(), result);
      return result;
    }
    std::wstring committedNative() const
    {
      std::string logical;
      std::wstring desired;
      LOKA_VERIFY(node->document.project(logical) == EDITOR_OK);
      LOKA_VERIFY(loka::win32::TextEditorToWide(logical, desired));
      return desired;
    }
    void matches() const
    {
      const std::wstring desired = committedNative();
      LOKA_VERIFY(native() == desired);
      LOKA_VERIFY(GetWindowTextLengthW(context->hwnd()) == static_cast<int>(desired.size()));
    }
  };
  void printUndoText(const char *label, const std::wstring &wide, const std::string &before)
  {
    // Escape UTF-16 units so non-ASCII and line endings survive console encoding.
    std::printf("[undo failure] %s UTF-16=", label);
    for (std::size_t i = 0; i < wide.size(); ++i)
    {
      const unsigned unit = static_cast<unsigned>(wide[i]);
      if (unit >= 32 && unit < 127 && unit != '\\')
        std::putchar(static_cast<int>(unit));
      else
        std::printf("\\u%04x", unit);
    }
    std::printf("\n");
    std::string logical;
    if (!loka::win32::TextEditorFromWide(wide.data(), wide.size(), logical))
    {
      std::printf("[undo failure] %s diff unavailable: non-ASCII/native conversion refused\n", label);
      return;
    }
    const loka::app::TextEditorLineDiff diff = loka::app::DiffTextEditorLines(before, logical);
    const char *kind = diff.before() == 0 && diff.after() == 0   ? "unchanged"
                       : diff.before() == 1 && diff.after() == 1 ? "single-line"
                       : diff.before() == 1 && diff.after() == 2 ? "split-candidate"
                       : diff.before() == 2 && diff.after() == 1 ? "join-candidate"
                                                                 : "unsupported-range";
    std::printf("[undo failure] %s diff first=%d last-before=%d last-after=%d before=%d after=%d kind=%s\n",
                label,
                diff.first(),
                diff.before() ? diff.first() + diff.before() - 1 : -1,
                diff.after() ? diff.first() + diff.after() - 1 : -1,
                diff.before(),
                diff.after(),
                kind);
  }
  void paste(HWND window, const wchar_t *text)
  {
    const std::wstring value(text);
    HGLOBAL storage = GlobalAlloc(GMEM_MOVEABLE, (value.size() + 1) * sizeof(wchar_t));
    LOKA_VERIFY(storage);
    wchar_t *target = static_cast<wchar_t *>(GlobalLock(storage));
    LOKA_VERIFY(target);
    for (std::size_t i = 0; i <= value.size(); ++i)
      target[i] = value.c_str()[i];
    GlobalUnlock(storage);
    LOKA_VERIFY(OpenClipboard(window));
    LOKA_VERIFY(EmptyClipboard());
    LOKA_VERIFY(SetClipboardData(CF_UNICODETEXT, storage));
    LOKA_VERIFY(CloseClipboard());
    SendMessageW(window, WM_PASTE, 0, 0);
  }
  struct Snapshot
  {
    ListRevision revision;
    LineCursor cursor;
    std::string text;
    std::vector<ItemId> ids;
    explicit Snapshot(const Fixture &fixture)
        : revision(fixture.lines.revision().get()),
          cursor(fixture.cursor.get())
    {
      LOKA_VERIFY(fixture.node->document.project(text) == EDITOR_OK);
      for (unsigned short i = 0; i < fixture.lines.size(); ++i)
        ids.push_back(fixture.lines.at(i).id);
    }
    void unchanged(const Fixture &fixture) const
    {
      LOKA_VERIFY(!(revision != fixture.lines.revision().get()) && cursor == fixture.cursor.get());
      std::string actual;
      LOKA_VERIFY(fixture.node->document.project(actual) == EDITOR_OK && text == actual);
      LOKA_VERIFY(ids.size() == fixture.lines.size());
      for (unsigned short i = 0; i < fixture.lines.size(); ++i)
        LOKA_VERIFY(ids[i] == fixture.lines.at(i).id);
    }
  };
  struct Observer
  {
    Fixture &fixture;
    unsigned notifications, settled;
    bool nested, deferred;
    explicit Observer(Fixture &value)
        : fixture(value),
          notifications(0),
          settled(0),
          nested(false),
          deferred(false)
    {
      const_cast<State<ListRevision> &>(fixture.lines.revision()).bind(&changed, this, false);
    }
    ~Observer()
    {
      const_cast<State<ListRevision> &>(fixture.lines.revision()).unbind(&changed, this);
    }
    static void changed(void *data)
    {
      Observer &observer = *static_cast<Observer *>(data);
      ++observer.notifications;
      const PaintQuery query = {Win32RetirableContext::paintScope(), PLACEMENT_ELIGIBLE};
      const PaintAnswer answer = observer.fixture.context->queryPaintDamage(query);
      LOKA_VERIFY(answer.kind == PAINT_ANSWER_EXACT);
      observer.fixture.tracker.defer(&flushed, &observer);
      if (observer.nested)
      {
        observer.nested = false;
        const Snapshot before(observer.fixture);
        observer.fixture.type(L'Z');
        before.unchanged(observer.fixture);
      }
    }
    static void flushed(void *data)
    {
      Observer &observer = *static_cast<Observer *>(data);
      ++observer.settled;
      if (observer.deferred)
      {
        observer.deferred = false;
        const Snapshot before(observer.fixture);
        observer.fixture.type(L'Z');
        before.unchanged(observer.fixture);
      }
    }
  };
  void restored(Fixture &fixture, const Snapshot &snapshot, const Observer &observer)
  {
    snapshot.unchanged(fixture);
    fixture.matches();
    LOKA_VERIFY(observer.notifications == 0 && observer.settled == 0);
    LOKA_VERIFY(EditorAccess::restores(*fixture.context) == 1);
    LOKA_VERIFY(!SendMessageW(fixture.context->hwnd(), EM_CANUNDO, 0, 0));
    DWORD start = 0, end = 0;
    SendMessageW(fixture.context->hwnd(), EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
    LOKA_VERIFY(start == 2 && end == 2);
  }
} // namespace

void testWin32TextEditorConversion()
{
  std::wstring wide;
  std::string logical;
  LOKA_VERIFY(loka::win32::TextEditorToWide("first\r\rlast\r", wide));
  LOKA_VERIFY(wide == L"first\r\n\r\nlast\r\n");
  LOKA_VERIFY(loka::win32::TextEditorFromWide(wide.data(), wide.size(), logical));
  LOKA_VERIFY(logical == "first\r\rlast\r");
  LOKA_VERIFY(loka::win32::TextEditorFromWide(L"a\rb\nc\r\nd", 8, logical) && logical == "a\rb\rc\rd");
  LOKA_VERIFY(loka::win32::TextEditorToWide(std::string(8192, 'a'), wide) && wide.size() == 8192);
  LOKA_VERIFY(loka::win32::TextEditorFromWide(wide.data(), wide.size(), logical) && logical.size() == 8192);
  LOKA_VERIFY(!loka::win32::TextEditorFromWide(L"\xff21", 1, logical));
  LOKA_VERIFY(!loka::win32::TextEditorToWide(std::string(1, static_cast<char>(0x80)), wide));
}
void testWin32TextEditorActionsUseLineQueries()
{
  Fixture fixture(128);
  Probe probe(fixture.context->hwnd());
  Observer observer(fixture);
  const ItemId first = fixture.lines.at(0).id, second = fixture.lines.at(1).id;
  const ListRevision before = fixture.lines.revision().get();
  fixture.type(L'x');
  LOKA_VERIFY(observer.notifications == 1 && observer.settled == 1);
  LOKA_VERIFY(fixture.lines.revision().get().content == before.content + 1);
  LOKA_VERIFY(fixture.lines.revision().get().structure == before.structure);
  LOKA_VERIFY(fixture.lines.revision().get().change.kind == LIST_UPDATE);
  LOKA_VERIFY(fixture.lines.at(0).value.equals(String("abxcd")) && fixture.cursor.get() == LineCursor(first, 3));
  LOKA_VERIFY(probe.lineReads == 0 && probe.textReads == 1);
  fixture.type(L'\r');
  LOKA_VERIFY(observer.notifications == 2 && observer.settled == 2);
  LOKA_VERIFY(fixture.lines.size() == 129 && fixture.lines.at(2).id == second);
  LOKA_VERIFY(fixture.lines.at(0).value.equals(String("abx")) && fixture.lines.at(1).value.equals(String("cd")));
  LOKA_VERIFY(fixture.cursor.get() == LineCursor(fixture.lines.at(1).id, 0));
  LOKA_VERIFY(fixture.lines.revision().get().change.kind == LIST_BATCH);
  fixture.type(L'\b');
  LOKA_VERIFY(observer.notifications == 3 && observer.settled == 3);
  LOKA_VERIFY(fixture.lines.size() == 128 && fixture.lines.at(1).id == second);
  LOKA_VERIFY(fixture.lines.revision().get().change.kind == LIST_BATCH);
  LOKA_VERIFY(fixture.cursor.get() == LineCursor(first, 3));
  fixture.matches();
  const unsigned sets = probe.sets;
  fixture.context->onPropsApplied();
  LOKA_VERIFY(probe.sets == sets && SendMessageW(fixture.context->hwnd(), EM_CANUNDO, 0, 0));
  const ListRevision beforeMove = fixture.lines.revision().get();
  SendMessageW(fixture.context->hwnd(), WM_KEYDOWN, VK_LEFT, 1);
  LOKA_VERIFY(fixture.cursor.get() == LineCursor(first, 2));
  LOKA_VERIFY(!(fixture.lines.revision().get() != beforeMove));
  SendMessageW(fixture.context->hwnd(), EM_SETSEL, 1, 3);
  SendMessageW(fixture.context->hwnd(), WM_KEYUP, VK_SHIFT, 1);
  fixture.context->onPropsApplied();
  DWORD selectionStart = 0, selectionEnd = 0;
  SendMessageW(fixture.context->hwnd(),
               EM_GETSEL,
               reinterpret_cast<WPARAM>(&selectionStart),
               reinterpret_cast<LPARAM>(&selectionEnd));
  LOKA_VERIFY(selectionStart == 1 && selectionEnd == 3);
  const PaintQuery echoQuery = {Win32RetirableContext::paintScope(), PLACEMENT_ELIGIBLE};
  LOKA_VERIFY(fixture.context->queryPaintDamage(echoQuery).kind == PAINT_ANSWER_EXACT);
  // Isolate this replacement from EDIT's prior typing/deletion undo record.
  SendMessageW(fixture.context->hwnd(), EM_EMPTYUNDOBUFFER, 0, 0);
  {
    ChangeObservation replacement(fixture.host);
    SendMessageW(fixture.context->hwnd(), EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(L"Q"));
    // Positive control: a zero undo count is meaningful only if this route fires.
    LOKA_VERIFY(replacement.arrivals() > 0);
    LOKA_VERIFY(replacement.native() == fixture.native());
  }
  LOKA_VERIFY(fixture.lines.at(0).value.equals(String("aQcd")));
  fixture.matches();
  const LRESULT distant = SendMessageW(fixture.context->hwnd(), EM_LINEINDEX, 10, 0);
  LOKA_VERIFY(distant >= 0);
  SendMessageW(fixture.context->hwnd(), EM_SETSEL, distant, distant);
  LOKA_VERIFY(SendMessageW(fixture.context->hwnd(), EM_CANUNDO, 0, 0));
  std::string beforeUndo;
  LOKA_VERIFY(fixture.node->document.project(beforeUndo) == EDITOR_OK);
  {
    ChangeObservation undo(fixture.host);
    const LRESULT undone = SendMessageW(fixture.context->hwnd(), EM_UNDO, 0, 0);
    // EDIT's single-level undo need not reconstruct the pre-replacement text.
    // The model must accept the actual native result and its final caret.
    const std::wstring native = fixture.native(); // Reads back with GetWindowTextW.
    const std::wstring nativeFirst = native.substr(0, native.find_first_of(L"\r\n"));
    std::string logicalFirst;
    const bool converted = loka::win32::TextEditorFromWide(nativeFirst.data(), nativeFirst.size(), logicalFirst);
    const bool lineMatches =
        converted && fixture.lines.at(0).value.equals(String::Utf8(logicalFirst.data(), logicalFirst.size()));
    DWORD caretStart = 0, caretEnd = 0;
    SendMessageW(
        fixture.context->hwnd(), EM_GETSEL, reinterpret_cast<WPARAM>(&caretStart), reinterpret_cast<LPARAM>(&caretEnd));
    const int caretLine = static_cast<int>(SendMessageW(fixture.context->hwnd(), EM_LINEFROMCHAR, caretEnd, 0));
    const int caretOffset = static_cast<int>(SendMessageW(fixture.context->hwnd(), EM_LINEINDEX, caretLine, 0));
    const bool caretMatches = caretLine >= 0 && caretLine < fixture.lines.size() && caretOffset >= 0
                              && fixture.cursor.get()
                                     == LineCursor(fixture.lines.at(static_cast<unsigned short>(caretLine)).id,
                                                   static_cast<int>(caretEnd) - caretOffset);
    if (!undone || !lineMatches || !caretMatches || EditorAccess::restores(*fixture.context) != 0
        || undo.arrivals() == 0)
    {
      const StringBuffer model = fixture.lines.at(0).value.bufferWithEncoding(StringEncodingUtf8);
      std::printf(
          "[undo failure] EM_UNDO=%ld EN_CHANGE=%u model line 0=%.*s restores=%u status=%d notification-status=%d\n",
          static_cast<long>(undone),
          undo.arrivals(),
          static_cast<int>(model.length()),
          static_cast<const char *>(model.data()),
          EditorAccess::restores(*fixture.context),
          static_cast<int>(EditorAccess::status(*fixture.context)),
          static_cast<int>(undo.status()));
      std::printf("[undo failure] native caret line=%d column=%d model cursor matches=%d\n",
                  caretLine,
                  static_cast<int>(caretEnd) - caretOffset,
                  caretMatches ? 1 : 0);
      printUndoText("after EM_UNDO", native, beforeUndo);
      if (undo.arrivals())
        printUndoText("at EN_CHANGE before dispatch", undo.native(), beforeUndo);
      std::fflush(stdout);
    }
    LOKA_VERIFY(undone);
    LOKA_VERIFY(lineMatches);
    LOKA_VERIFY(caretMatches);
    LOKA_VERIFY(undo.arrivals() > 0);
  }
  LOKA_VERIFY(fixture.lines.at(0).id == first && fixture.lines.size() == 128);
  LOKA_VERIFY(fixture.lines.at(10).value.equals(String("abcd")));
  LOKA_VERIFY(EditorAccess::restores(*fixture.context) == 0);
  fixture.matches();
  // A trailing empty logical line must preserve the model, caret and CRLF projection.
  const unsigned short lineCount = fixture.lines.size();
  const int nativeEnd = GetWindowTextLengthW(fixture.context->hwnd());
  SendMessageW(fixture.context->hwnd(), EM_SETSEL, nativeEnd, nativeEnd);
  fixture.type(L'\r');
  LOKA_VERIFY(fixture.lines.size() == lineCount + 1);
  const unsigned short last = static_cast<unsigned short>(fixture.lines.size() - 1);
  LOKA_VERIFY(fixture.lines.at(last).value.equals(String("")));
  LOKA_VERIFY(fixture.cursor.get() == LineCursor(fixture.lines.at(last).id, 0));
  const std::wstring trailing = fixture.native();
  LOKA_VERIFY(trailing.size() >= 2 && trailing.compare(trailing.size() - 2, 2, L"\r\n") == 0);
  LOKA_VERIFY(GetWindowTextLengthW(fixture.context->hwnd()) == static_cast<int>(fixture.committedNative().size()));
  fixture.matches();
  fixture.type(L'z');
  LOKA_VERIFY(fixture.lines.at(last).value.equals(String("z")));
  LOKA_VERIFY(fixture.cursor.get() == LineCursor(fixture.lines.at(last).id, 1));
  fixture.matches();
}
void testWin32TextEditorRefusalRestoresAndClearsUndo()
{
  {
    Fixture fixture;
    Observer observer(fixture);
    const Snapshot before(fixture);
    paste(fixture.context->hwnd(), L"\r\n");
    restored(fixture, before, observer);
    paste(fixture.context->hwnd(), L"Q");
    LOKA_VERIFY(fixture.lines.at(0).value.equals(String("abQcd")));
    fixture.matches();
  }
  {
    Fixture fixture;
    Observer observer(fixture);
    const Snapshot before(fixture);
    paste(fixture.context->hwnd(), L"Q\r\nR");
    restored(fixture, before, observer);
  }
  {
    Fixture fixture;
    Observer observer(fixture);
    const Snapshot before(fixture);
    SendMessageW(fixture.context->hwnd(), EM_SETSEL, 1, 8);
    SendMessageW(fixture.context->hwnd(), EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(L"Q\r\nR"));
    restored(fixture, before, observer);
  }

  {
    Fixture fixture(256);
    Probe probe(fixture.context->hwnd());
    probe.seedUndo = true;
    Observer observer(fixture);
    SendMessageW(fixture.context->hwnd(), EM_LINESCROLL, 0, 80);
    const LRESULT visible = SendMessageW(fixture.context->hwnd(), EM_GETFIRSTVISIBLELINE, 0, 0);
    LOKA_VERIFY(visible > 0);
    const Snapshot before(fixture);
    fixture.type(L'\r');
    restored(fixture, before, observer);
    LOKA_VERIFY(probe.sets == 1 && !probe.restoredInsideNotification);
    LOKA_VERIFY(SendMessageW(fixture.context->hwnd(), EM_GETFIRSTVISIBLELINE, 0, 0) == visible);
    fixture.type(L'x');
    LOKA_VERIFY(observer.notifications == 1);
    fixture.matches();
  }
  {
    Fixture fixture;
    Observer observer(fixture);
    const Snapshot before(fixture);
    fixture.type(static_cast<wchar_t>(0xff21));
    restored(fixture, before, observer);
    fixture.type(L'x');
    LOKA_VERIFY(observer.notifications == 1);
  }
  {
    Fixture fixture;
    Observer observer(fixture);
    const Snapshot before(fixture);
    const std::wstring oversized(8193, L'x');
    SendMessageW(fixture.context->hwnd(), EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(oversized.c_str()));
    restored(fixture, before, observer);
    fixture.type(L'x');
    LOKA_VERIFY(observer.notifications == 1);
  }
  {
    Fixture fixture(1, std::string(8192, 'a'));
    Observer observer(fixture);
    const Snapshot before(fixture);
    fixture.type(L'x');
    restored(fixture, before, observer);
  }
  {
    Fixture fixture;
    Observer observer(fixture);
    const Snapshot before(fixture);
    loka::core::testing::failLokaAllocRaw("TextEditor", "Scratch", 1);
    fixture.type(L'\r');
    loka::core::testing::allowLokaAllocRaw();
    restored(fixture, before, observer);
    fixture.type(L'x');
    LOKA_VERIFY(observer.notifications == 1);
  }
  {
    Fixture fixture;
    Observer observer(fixture);
    const Snapshot before(fixture);
    // An unsolicited notification has no subclass input stack to unwind.
    // Its refusal stays read-only until a separate timer dispatch.
    LOKA_VERIFY(SetWindowTextW(fixture.context->hwnd(), L"bad\xff21"));
    before.unchanged(fixture);
    LOKA_VERIFY(observer.notifications == 0 && !EditorAccess::pending(*fixture.context));
    SendMessageW(fixture.host, WM_COMMAND, MAKEWPARAM(0, EN_CHANGE), reinterpret_cast<LPARAM>(fixture.context->hwnd()));
    before.unchanged(fixture);
    LOKA_VERIFY(EditorAccess::pending(*fixture.context) && EditorAccess::restores(*fixture.context) == 0);
    SendMessageW(fixture.context->hwnd(), WM_TIMER, 853, 0);
    restored(fixture, before, observer);
    fixture.type(L'x');
    LOKA_VERIFY(observer.notifications == 1);
  }
  {
    Fixture fixture;
    LOKA_VERIFY(fixture.lines.detach() == EDIT_OK);
    LOKA_VERIFY(fixture.lines.attach(&fixture.tracker, 256) == ATTACH_OK);
    LOKA_VERIFY(fixture.lines.insert(0, String("fresh")) == EDIT_OK);
    Observer observer(fixture);
    const Snapshot before(fixture);
    fixture.type(L'x');
    restored(fixture, before, observer);
    fixture.type(L'y');
    LOKA_VERIFY(fixture.lines.at(0).value.equals(String("fryesh")));
  }
  {
    Fixture fixture(1);
    for (unsigned int i = 1; i < 65535; ++i)
    {
      ItemId temporary;
      LOKA_VERIFY(fixture.lines.insert(1, String(), &temporary) == EDIT_OK);
      LOKA_VERIFY(fixture.lines.remove(temporary) == EDIT_OK);
    }
    fixture.context->onPropsApplied();
    Observer observer(fixture);
    const Snapshot before(fixture);
    fixture.type(L'\r');
    restored(fixture, before, observer);
    fixture.type(L'x');
    LOKA_VERIFY(observer.notifications == 1);
  }
  {
    Fixture fixture(1, std::string(8191, 'a'));
    fixture.type(L'x');
    const Snapshot committed(fixture);
    const int nativeUnits = GetWindowTextLengthW(fixture.context->hwnd());
    if (committed.text.size() != 8192 || nativeUnits != 8192 || EditorAccess::restores(*fixture.context) != 0
        || EditorAccess::status(*fixture.context) != EDITOR_OK)
    {
      std::printf("[cap failure] committed logical=%lu native UTF-16=%d restores=%u status=%d\n",
                  static_cast<unsigned long>(committed.text.size()),
                  nativeUnits,
                  EditorAccess::restores(*fixture.context),
                  static_cast<int>(EditorAccess::status(*fixture.context)));
      std::fflush(stdout);
    }
    std::string line;
    LOKA_VERIFY(loka::platform::CollectUtf8(fixture.lines.at(0).value, line));
    LOKA_VERIFY(line.size() == 8192 && committed.text.size() == 8192 && nativeUnits == 8192);
    LOKA_VERIFY(EditorAccess::status(*fixture.context) == EDITOR_OK);
    fixture.matches();
    LOKA_VERIFY(GetWindowTextLengthW(fixture.context->hwnd()) == static_cast<int>(fixture.committedNative().size()));
    LOKA_VERIFY(EditorAccess::restores(*fixture.context) == 0);
  }
  {
    // 8191 logical bytes across two lines; one insertion reaches the cap.
    Fixture fixture(2, std::string(4095, 'a'));
    fixture.type(L'x');
    const Snapshot before(fixture);
    LOKA_VERIFY(before.text.size() == 8192);
    LOKA_VERIFY(fixture.committedNative().size() == 8193);
    fixture.matches();
    Observer observer(fixture);
    fixture.type(L'x');
    before.unchanged(fixture);
    fixture.matches();
    LOKA_VERIFY(observer.notifications == 0 && observer.settled == 0);
    LOKA_VERIFY(EditorAccess::restores(*fixture.context) == 1 && !EditorAccess::pending(*fixture.context));
    LOKA_VERIFY(!SendMessageW(fixture.context->hwnd(), EM_CANUNDO, 0, 0));
  }
  {
    Fixture fixture(257, "a", 257);
    LOKA_VERIFY(EditorAccess::status(*fixture.context) == EDITOR_CAPACITY && fixture.native().empty());
    LOKA_VERIFY(GetWindowLongPtrW(fixture.context->hwnd(), GWL_STYLE) & ES_READONLY);
    LOKA_VERIFY(fixture.lines.remove(fixture.lines.at(256).id) == EDIT_OK);
    fixture.context->onPropsApplied();
    LOKA_VERIFY(EditorAccess::status(*fixture.context) == EDITOR_OK);
    fixture.type(L'x');
    fixture.matches();
    LOKA_VERIFY(fixture.lines.insert(256, String("a")) == EDIT_OK);
    fixture.context->onPropsApplied();
    LOKA_VERIFY(EditorAccess::status(*fixture.context) == EDITOR_CAPACITY && fixture.native().empty());
  }
}
void testWin32TextEditorNestedInput()
{
  for (int deferred = 0; deferred < 2; ++deferred)
  {
    Fixture fixture;
    Observer observer(fixture);
    observer.nested = deferred == 0;
    observer.deferred = deferred != 0;
    Probe probe(fixture.context->hwnd());
    probe.echo = true;
    fixture.type(L'x');
    LOKA_VERIFY(observer.notifications == 1 && observer.settled == 1);
    LOKA_VERIFY(fixture.lines.at(0).value.equals(String("abxcd")));
    fixture.matches();
    LOKA_VERIFY(EditorAccess::restores(*fixture.context) == 1 && probe.sets == 1 && !probe.restoredInsideNotification);
    LOKA_VERIFY(!SendMessageW(fixture.context->hwnd(), EM_CANUNDO, 0, 0));
    fixture.type(L'y');
    LOKA_VERIFY(fixture.lines.at(0).value.equals(String("abxycd")) && observer.notifications == 2);
  }
}
void testWin32TextEditorFailedReplacementRetries()
{
  for (int failure = 0; failure < 3; ++failure)
  {
    Fixture fixture(256);
    Probe probe(fixture.context->hwnd());
    const loka::win32::testing::TextEditorSetFailure failures[] = {
        loka::win32::testing::TEXT_EDITOR_SET_REFUSED,
        loka::win32::testing::TEXT_EDITOR_SET_TRUNCATED,
        loka::win32::testing::TEXT_EDITOR_SET_FALSE_AFTER_DELIVERY};
    loka::win32::testing::failTextEditorSets(failures[failure], failure == 0 ? 2 : 1);
    const Snapshot before(fixture);
    fixture.type(L'\r');
    before.unchanged(fixture);
    LOKA_VERIFY(EditorAccess::pending(*fixture.context));
    LOKA_VERIFY(GetWindowLongPtrW(fixture.context->hwnd(), GWL_STYLE) & ES_READONLY);
    LOKA_VERIFY(EditorAccess::restores(*fixture.context) == 1 && !probe.restoredInsideNotification);
    fixture.type(L'x');
    before.unchanged(fixture);
    // Explicitly dispatch each timer turn; no nested pump inside owner apply.
    SendMessageW(fixture.context->hwnd(), WM_TIMER, 853, 0);
    LOKA_VERIFY(EditorAccess::restores(*fixture.context) == 2);
    if (failure == 0)
    {
      LOKA_VERIFY(EditorAccess::pending(*fixture.context));
      SendMessageW(fixture.context->hwnd(), WM_TIMER, 853, 0);
      LOKA_VERIFY(EditorAccess::restores(*fixture.context) == 3);
    }
    LOKA_VERIFY(!EditorAccess::pending(*fixture.context));
    LOKA_VERIFY(!(GetWindowLongPtrW(fixture.context->hwnd(), GWL_STYLE) & ES_READONLY));
    fixture.matches();
    fixture.type(L'x');
    LOKA_VERIFY(fixture.lines.at(0).value.equals(String("abxcd")));
  }
}
void testWin32TextEditorLayoutDpiAndRetirement()
{
  PlainOnlyHighlighter highlighter;
  Fixture fixture;
  fixture.node->props.highlighter(highlighter);
  fixture.context->onPropsApplied();
  fixture.type(L'x');
  LOKA_VERIFY(highlighter.calls == 0);
  HWND child = fixture.context->hwnd();
  const LONG_PTR style = GetWindowLongPtrW(child, GWL_STYLE);
  const LONG_PTR required = ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_VSCROLL;
  LOKA_VERIFY((style & required) == required);
  LayoutState bounds;
  bounds.x = 10;
  bounds.y = 20;
  bounds.width = 300;
  bounds.height = 140;
  bounds.spacing = 4;
  LOKA_VERIFY(fixture.context->layout(&fixture.controller, bounds) == 164 && bounds.height == 140);
  RECT rect;
  LOKA_VERIFY(GetWindowRect(child, &rect) && rect.bottom - rect.top == 140);
  const HFONT font = reinterpret_cast<HFONT>(SendMessageW(child, WM_GETFONT, 0, 0));
  LOKA_VERIFY(font);
  fixture.controller.updateDisplayScale(loka::win32::Win32DisplayScale(144, RailMetrics()));
  const HFONT nextFont = reinterpret_cast<HFONT>(SendMessageW(child, WM_GETFONT, 0, 0));
  LOKA_VERIFY(nextFont && nextFont != font);
  const PaintQuery query = {Win32RetirableContext::paintScope(), PLACEMENT_ELIGIBLE};
  const PaintAnswer answer = fixture.context->queryPaintDamage(query);
  LOKA_VERIFY(answer.kind == PAINT_ANSWER_NATIVE_SCHEDULED);
  {
    Probe probe(child);
    loka::win32::testing::failTextEditorSets(loka::win32::testing::TEXT_EDITOR_SET_REFUSED, 1);
    fixture.type(static_cast<wchar_t>(0xff21));
    LOKA_VERIFY(EditorAccess::pending(*fixture.context));
    const Snapshot before(fixture);
    NotifySubtreeNodeDetached(fixture.node);
    const unsigned beforeDetachSets = probe.sets;
    SendMessageW(child, WM_TIMER, 853, 0);
    SendMessageW(child, WM_CHAR, 'x', 1);
    before.unchanged(fixture);
    LOKA_VERIFY(probe.sets == beforeDetachSets);
    delete fixture.node;
    fixture.node = 0;
    fixture.context = 0;
    LOKA_VERIFY(!Win32TextEditorContext::fromWindow(child));
    fixture.controller.drainNativeRetirements();
    LOKA_VERIFY(!IsWindow(child) && !GetWindow(fixture.host, GW_CHILD));
  }
}
