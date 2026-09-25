#include "MacFocusTests.hpp"
#include "MacApp.hpp"
#include "MacObjCCompat.hpp"
#include "MacWindow.hpp"
#include "MacScenePlatformController.hpp"
#include "context/MacEditTextContext.hpp"
#include "context/MacTextEditorContext.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/controls/EditText.hpp"
#include "app/nodes/controls/TextEditor.hpp"
#include "app/scene/Scene.hpp"
#include "platform/null/NullPlatformContext.hpp"
#include "support/Headless.hpp"
#include "support/TestVerify.hpp"
#include "testing/MacWindowTestAccess.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#include <AppKit/AppKit.h>
#include <ApplicationServices/ApplicationServices.h>
#include <cstdio>

namespace
{
  using namespace loka::app;
  using namespace loka::app::scene;
  using namespace loka::core;
  typedef Focused<unsigned int> Fact;
  typedef loka::dsl::testing::MacWindowTestAccess NativeAccess;
  typedef loka::dsl::testing::SceneTestAccess SceneAccess;

  struct Facts : HeadlessStateOwner
  {
    Reported<Fact> focus;
    Reported<LineCursor> cursor;
    NodeState<String> firstText, secondText;
    ObservableList<String> lines;
    Facts()
    {
      StateBatchBase::CreateImmediateState(this, this->focus, Fact::none());
      StateBatchBase::CreateImmediateState(this, this->cursor, LineCursor::None());
      StateBatchBase::CreateImmediateState(this, this->firstText, String("first"));
      StateBatchBase::CreateImmediateState(this, this->secondText, String("second"));
      LOKA_VERIFY(this->lines.attach(this->tracker()->asPushTracker(), 4) == ATTACH_OK);
      LOKA_VERIFY(this->lines.insert(0, String("editor")) == EDIT_OK);
    }
  };
  class FocusRoot;
  struct FocusTag
  {
  };
  struct FocusProps : NodePropsBase<FocusProps>
  {
    typedef FocusRoot NodeType;
    typedef FocusTag TypeTag;
    Facts *facts;
    explicit FocusProps(Facts *value)
        : facts(value)
    {
    }
    bool operator<(const PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != this->propsTypeId())
        return this->propsTypeId() < rhs.propsTypeId();
      return this->facts < static_cast<const FocusProps &>(rhs).facts;
    }
  };
  class FocusRoot : public StdCompositionBoundaryNodeBase<FocusProps>
  {
    typedef StdCompositionBoundaryNodeBase<FocusProps> Base;

  public:
    typedef FocusTag TypeTag;
    explicit FocusRoot(const FocusProps &props)
        : Base(props)
    {
    }
    virtual void composeNode(NodeComposition &composition)
    {
      composition.declare(
          EditText(EditTextProps(this->props.facts->firstText).focusedAs(this->props.facts->focus, 1u)));
      composition.declare(
          EditText(EditTextProps(this->props.facts->secondText).focusedAs(this->props.facts->focus, 2u)));
      composition.declare(TextEditor(TextEditorProps(this->props.facts->lines, this->props.facts->cursor)
                                         .focusedAs(this->props.facts->focus, 3u)));
    }
  };
  typedef BoundaryDefinition<FocusProps, FocusRoot> FocusDefinition;
  class FocusApp : public MacApp
  {
  public:
    FocusApp()
        : MacApp(0)
    {
    }
    void install(MacWindow *window)
    {
      this->group_ = new AppComponentGroup(std::vector<AppComponent *>(1, window));
      window->setApp(this);
    }
  };
  NSTextField *field(Node *node)
  {
    LOKA_VERIFY(node && node->asEditTextNode() && node->getContext());
    return (NSTextField *)static_cast<MacEditTextContext *>(node->getContext())->nativeField();
  }
  NSTextView *editor(NSView *parent, NodeContext *context)
  {
    if (MacTextEditorContext::fromNativeFocus(parent) == context)
      return (NSTextView *)parent;
    for (NSUInteger i = 0; i < [[parent subviews] count]; ++i)
      if (NSTextView *found = editor([[parent subviews] objectAtIndex:i], context))
        return found;
    return nil;
  }

  /** Bound on waiting for AppKit to report a key-status change. */
  const double kKeyStatusWaitSeconds = 2.0;

  bool isKey(NSWindow *window)
  {
    return [window isKeyWindow] ? true : false;
  }

  /** Activation and key changes arrive through the event queue: deliver queued
      events until the window's key status matches, for a bounded time. */
  bool waitForKeyStatus(NSWindow *window, bool key)
  {
#if defined(MAC_OS_X_VERSION_MAX_ALLOWED) && (MAC_OS_X_VERSION_MAX_ALLOWED >= 101200)
    const NSEventMask anyEvent = NSEventMaskAny;
#else
    const NSUInteger anyEvent = NSAnyEventMask;
#endif
    NSDate *limit = [NSDate dateWithTimeIntervalSinceNow:kKeyStatusWaitSeconds];
    while (isKey(window) != key && [limit timeIntervalSinceNow] > 0)
    {
      NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
      NSEvent *event = [NSApp nextEventMatchingMask:anyEvent
                                          untilDate:[NSDate dateWithTimeIntervalSinceNow:0.05]
                                             inMode:NSDefaultRunLoopMode
                                            dequeue:YES];
      if (event)
        [NSApp sendEvent:event];
      [pool drain];
    }
    return isKey(window) == key;
  }

  /** An unbundled test process is not a regular application until it asks, as
      MacApp::run() does (apple/macos/src/MacApp.mm); only then can activation
      make its window key. */
  bool requestKeyWindow(NSWindow *window)
  {
    if ([NSApp respondsToSelector:@selector(setActivationPolicy:)])
      [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp activateIgnoringOtherApps:YES];
    [window makeKeyAndOrderFront:nil];
    return waitForKeyStatus(window, true);
  }

  /** Diagnostic only; the discriminating condition is the window's key status. */
  const char *keyRefusalReason()
  {
    bool locked = false;
    CFDictionaryRef session = CGSessionCopyCurrentDictionary();
    if (session)
    {
      // Not a documented key: present and true while the console screen is locked.
      id value = [(NSDictionary *)session objectForKey:@"CGSSessionScreenIsLocked"];
      locked = value && [value respondsToSelector:@selector(boolValue)] && [value boolValue];
      CFRelease(session);
    }
    if (locked)
      return "the console screen is locked";
    return [NSApp isActive] ? "the application is active but the window is not key"
                            : "the application was not activated";
  }
} // namespace

void testMacFocusReadAndCompletion()
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  [NSApplication sharedApplication];
  {
    Facts facts;
    NullPlatformContext context;
    FocusApp app;
    WindowProps props;
    props.scene(new Scene(FocusDefinition(FocusProps(&facts))));
    MacWindow *window = new MacWindow(&context, props);
    app.install(window);
    NSWindow *native = (NSWindow *)NativeAccess::nativeWindow(*window);
    LOKA_VERIFY(native != nil);
    // Key status decides which pins can run; the branch taken is always printed.
    const bool key = requestKeyWindow(native);
    Scene &scene = *window->scene();
    Node *root = SceneAccess::rootNode(scene);
    loka::dsl::CompositionCursor<Node> children(root->asNestable()->childrenHead(),
                                                root->asNestable()->childrenCount());
    Node *firstNode = children.next();
    Node *secondNode = children.next();
    Node *editorNode = children.next();
    NSTextField *first = field(firstNode);
    NSTextField *second = field(secondNode);
    // editor() itself resolves the TextEditor view through its class-checked hop.
    NSTextView *textEditor = editor((NSView *)NativeAccess::contentView(*window), editorNode->getContext());
    LOKA_VERIFY(textEditor && editorNode && editorNode->getContext());
    MacScenePlatformController *rail =
        static_cast<MacScenePlatformController *>(SceneAccess::platformController(scene));
    NodeContext *read = 0;

    // Class-checked hops need no key status: each resolves only a marked native
    // participant through its typed delegate owner.
    LOKA_VERIFY(MacEditTextContext::fromNativeFocus(first) == firstNode->getContext());
    id firstDelegate = [first delegate];
    [first setDelegate:(id)native];
    LOKA_VERIFY(MacEditTextContext::fromNativeFocus(first) == 0);
    [first setDelegate:firstDelegate];
    // A foreign field cannot acquire the participant mark by borrowing a delegate.
    NSTextField *foreignField = [[NSTextField alloc] initWithFrame:NSZeroRect];
    [foreignField setDelegate:firstDelegate];
    LOKA_VERIFY(MacEditTextContext::fromNativeFocus(foreignField) == 0);
    [foreignField setDelegate:nil];
    [foreignField release];
    id editorDelegate = [textEditor delegate];
    [textEditor setDelegate:(id)native];
    LOKA_VERIFY(MacTextEditorContext::fromNativeFocus(textEditor) == 0);
    [textEditor setDelegate:editorDelegate];
    // A foreign view carrying the genuine delegate still lacks the typed mark.
    NSTextView *foreignView = [[NSTextView alloc] initWithFrame:NSMakeRect(0, 0, 80, 24)];
    [foreignView setDelegate:editorDelegate];
    LOKA_VERIFY(MacTextEditorContext::fromNativeFocus(foreignView) == 0);
    [foreignView setDelegate:nil];
    [foreignView release];
    // An editing field's first responder is the field editor, whose delegate is the field.
    LOKA_VERIFY([native makeFirstResponder:first]);
    LOKA_VERIFY([first currentEditor] != nil);
    LOKA_VERIFY(MacEditTextContext::fromNativeFocus([native firstResponder]) == firstNode->getContext());

    if (key)
    {
      // Only completion publishes: the raw read answers before the fact moves.
      LOKA_VERIFY(rail->readNativeFocus(read) && read == firstNode->getContext());
      LOKA_VERIFY(!(facts.focus.state()->get() != Fact::none()));
      app.flushInvalidationsTick();
      LOKA_VERIFY(!(facts.focus.state()->get() != Fact(1u)));

      // Drive the field editor's Tab command through AppKit's key-view loop.
      [first setNextKeyView:second];
      [(NSTextView *)[first currentEditor] insertTab:nil];
      LOKA_VERIFY([second currentEditor] != nil);
      LOKA_VERIFY(!(facts.focus.state()->get() != Fact(1u)));
      app.flushInvalidationsTick();
      LOKA_VERIFY(!(facts.focus.state()->get() != Fact(2u)));

      // Exercise today's native capture/restore without replacing the Scene.
      LOKA_VERIFY([native makeFirstResponder:first]);
      app.flushInvalidationsTick();
      LOKA_VERIFY(!(facts.focus.state()->get() != Fact(1u)));
      LOKA_VERIFY([native makeFirstResponder:second]);
      rail->onChange(root, NODE_DIRTY_LAYOUT, true);
      LOKA_VERIFY([field(secondNode) currentEditor] != nil);
      app.flushInvalidationsTick();
      LOKA_VERIFY(!(facts.focus.state()->get() != Fact(2u)));

      // Another window of this application takes key status. The Loka window
      // cannot answer, so its fact is HELD while its first responder changes.
      NSWindow *other = [[NSWindow alloc] initWithContentRect:NSMakeRect(40, 40, 160, 80)
                                                    styleMask:LOKA_MAC_WINDOW_STYLE_TITLED
                                                      backing:NSBackingStoreBuffered
                                                        defer:NO];
      [other makeKeyAndOrderFront:nil];
      LOKA_VERIFY(waitForKeyStatus(native, false));
      LOKA_VERIFY([native makeFirstResponder:nil]);
      LOKA_VERIFY(!rail->readNativeFocus(read));
      app.flushInvalidationsTick();
      LOKA_VERIFY(!(facts.focus.state()->get() != Fact(2u)));
      [other orderOut:nil];
      [other release];
      [native makeKeyWindow];
      LOKA_VERIFY(waitForKeyStatus(native, true));
      app.flushInvalidationsTick();
      LOKA_VERIFY(!(facts.focus.state()->get() != Fact::none()));

      textEditor = editor((NSView *)NativeAccess::contentView(*window), editorNode->getContext());
      LOKA_VERIFY(textEditor && [native makeFirstResponder:textEditor]);
      LOKA_VERIFY(rail->readNativeFocus(read) && read == editorNode->getContext());
      app.flushInvalidationsTick();
      LOKA_VERIFY(!(facts.focus.state()->get() != Fact(3u)));

      // A first responder without the typed mark answers none, genuine delegate or not.
      NSTextView *foreign = [[NSTextView alloc] initWithFrame:NSMakeRect(0, 0, 80, 24)];
      [[native contentView] addSubview:foreign];
      [foreign setDelegate:[textEditor delegate]];
      LOKA_VERIFY([native makeFirstResponder:foreign]);
      LOKA_VERIFY(rail->readNativeFocus(read) && read == 0);
      [foreign setDelegate:nil];
      app.flushInvalidationsTick();
      LOKA_VERIFY(!(facts.focus.state()->get() != Fact::none()));
      [native makeFirstResponder:nil];
      [foreign removeFromSuperview];
      [foreign release];

      // Native mouse delivery selects the field; only completion publishes.
#if defined(MAC_OS_X_VERSION_MAX_ALLOWED) && (MAC_OS_X_VERSION_MAX_ALLOWED >= 101200)
      const NSEventType mouseUpType = NSEventTypeLeftMouseUp;
      const NSEventType mouseDownType = NSEventTypeLeftMouseDown;
#else
      const NSEventType mouseUpType = NSLeftMouseUp;
      const NSEventType mouseDownType = NSLeftMouseDown;
#endif
      first = field(firstNode);
      [first setFrame:NSMakeRect(10, 10, 160, 24)];
      const NSPoint point = [first convertPoint:NSMakePoint(8, 12) toView:nil];
      NSEvent *up = [NSEvent mouseEventWithType:mouseUpType
                                       location:point
                                  modifierFlags:0
                                      timestamp:0
                                   windowNumber:[native windowNumber]
                                        context:nil
                                    eventNumber:1
                                     clickCount:1
                                       pressure:0];
      [NSApp postEvent:up atStart:YES];
      NSEvent *down = [NSEvent mouseEventWithType:mouseDownType
                                         location:point
                                    modifierFlags:0
                                        timestamp:0
                                     windowNumber:[native windowNumber]
                                          context:nil
                                      eventNumber:0
                                       clickCount:1
                                         pressure:1];
      [native sendEvent:down];
      LOKA_VERIFY([first currentEditor] != nil);
      LOKA_VERIFY(!(facts.focus.state()->get() != Fact::none()));
      app.flushInvalidationsTick();
      LOKA_VERIFY(!(facts.focus.state()->get() != Fact(1u)));
    }
    else
    {
      std::printf("[skip] macOS focus pins that need a key window (raw read and publication at "
                  "completion, Tab, rebuild, HELD across key loss, TextEditor publication, "
                  "unmarked responder, click): window not key after %.1f s because %s; the "
                  "class-checked hops and the non-key read still run.\n",
                  kKeyStatusWaitSeconds, keyRefusalReason());
      std::fflush(stdout);
      // HELD: a window that is not key cannot answer, whoever is its first responder.
      LOKA_VERIFY(!rail->readNativeFocus(read));
      app.flushInvalidationsTick();
      LOKA_VERIFY(!(facts.focus.state()->get() != Fact::none()));
    }

    // AppKit may outlive the logical contexts; retained native objects lose owners.
    first = field(firstNode);
    [first retain];
    [textEditor retain];
    {
      StateTrackerGuard guard(window->getTracker());
      window->visibilityState().set(false);
    }
    app.flushInvalidationsTick();
    LOKA_VERIFY(NativeAccess::nativeWindow(*window) == 0);
    LOKA_VERIFY(MacEditTextContext::fromNativeFocus(first) == 0);
    LOKA_VERIFY(MacTextEditorContext::fromNativeFocus(textEditor) == 0);
    LOKA_VERIFY(!(facts.focus.state()->get() != Fact::none()));
    [first release];
    [textEditor release];
  }
  [pool drain];
}
