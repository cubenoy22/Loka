#include "Win32EditTextBridgeTests.hpp"
#include "support/TestVerify.hpp"
#include "support/LokaAllocFailure.hpp"
#include "context/Win32TextEditorContext.hpp"
#include "context/Win32EditTextBridge.hpp"
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

  LRESULT CALLBACK HostProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
  {
    Win32ScenePlatformController *controller =
        static_cast<Win32ScenePlatformController *>(GetPropW(window, kHostController));
    if (message == WM_COMMAND && controller)
    {
      HANDLE previousNotification = GetPropW(window, kNotification);
      LOKA_VERIFY(SetPropW(window, kNotification, controller));
      const bool handled = controller->handleCommand(wParam, lParam);
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
  /** Native message instrumentation, with refusal at the actual setter door.
      No production allocation or native-setter test hooks are needed. */
  struct Probe
  {
    HWND window;
    WNDPROC previous;
    unsigned lineReads, textReads, sets;
    unsigned failSets;
    bool truncateSet, echo, restoredInsideNotification, seedUndo, falseAfterDelivery;
    explicit Probe(HWND value)
        : window(value),
          previous(0),
          lineReads(0),
          textReads(0),
          sets(0),
          failSets(0),
          truncateSet(false),
          echo(false),
          restoredInsideNotification(false),
          seedUndo(false),
          falseAfterDelivery(false)
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
        if (probe.failSets)
        {
          --probe.failSets;
          return FALSE;
        }
        if (probe.truncateSet)
        {
          probe.truncateSet = false;
          return CallWindowProcW(probe.previous, window, message, wParam, reinterpret_cast<LPARAM>(L""));
        }
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
      if (message == WM_SETTEXT && probe.falseAfterDelivery)
      {
        probe.falseAfterDelivery = false;
        return FALSE;
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
      delete node;
      controller.drainNativeRetirements();
      RemovePropW(host, kHostController);
      LOKA_VERIFY(DestroyWindow(host));
    }
    void type(wchar_t value)
    {
      SendMessageW(context->hwnd(), WM_CHAR, value, 1);
    }
    std::wstring native() const
    {
      std::wstring result;
      loka::win32::ReadEditTextWide(context->hwnd(), result);
      return result;
    }
    void matches() const
    {
      std::string logical;
      std::wstring desired;
      LOKA_VERIFY(node->document.project(logical) == EDITOR_OK);
      LOKA_VERIFY(loka::win32::TextEditorToWide(logical, desired));
      LOKA_VERIFY(native() == desired);
    }
  };
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
  LOKA_VERIFY(probe.lineReads == 1 && probe.textReads == 0);
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
  SendMessageW(fixture.context->hwnd(), EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(L"Q"));
  LOKA_VERIFY(fixture.lines.at(0).value.equals(String("aQcd")));
  fixture.matches();
  const LRESULT distant = SendMessageW(fixture.context->hwnd(), EM_LINEINDEX, 10, 0);
  SendMessageW(fixture.context->hwnd(), EM_SETSEL, distant, distant);
  SendMessageW(fixture.context->hwnd(), WM_UNDO, 0, 0);
  LOKA_VERIFY(fixture.lines.at(0).value.equals(String("abxcd")));
  fixture.matches();
}
void testWin32TextEditorRefusalRestoresAndClearsUndo()
{
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
    fixture.matches();
    LOKA_VERIFY(GetWindowTextLengthW(fixture.context->hwnd()) == 8192);
    LOKA_VERIFY(EditorAccess::restores(*fixture.context) == 0);
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
    probe.failSets = failure == 0 ? 2 : 0;
    probe.truncateSet = failure == 1;
    probe.falseAfterDelivery = failure == 2;
    const Snapshot before(fixture);
    fixture.type(L'\r');
    before.unchanged(fixture);
    LOKA_VERIFY(EditorAccess::pending(*fixture.context));
    LOKA_VERIFY(GetWindowLongPtrW(fixture.context->hwnd(), GWL_STYLE) & ES_READONLY);
    LOKA_VERIFY(probe.sets == 1 && !probe.restoredInsideNotification);
    fixture.type(L'x');
    before.unchanged(fixture);
    // Explicitly dispatch each timer turn; no nested pump inside owner apply.
    SendMessageW(fixture.context->hwnd(), WM_TIMER, 853, 0);
    LOKA_VERIFY(probe.sets == 2);
    if (failure == 0)
    {
      LOKA_VERIFY(EditorAccess::pending(*fixture.context));
      SendMessageW(fixture.context->hwnd(), WM_TIMER, 853, 0);
      LOKA_VERIFY(probe.sets == 3);
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
    probe.failSets = 1;
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
