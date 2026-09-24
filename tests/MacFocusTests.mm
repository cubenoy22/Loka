#include "MacFocusTests.hpp"
#include "MacApp.hpp"
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
} // namespace

void testMacFocusReadAndCompletion()
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  [NSApplication sharedApplication];
  [NSApp activateIgnoringOtherApps:YES];
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
    [native makeKeyAndOrderFront:nil];
    // An interactive AppKit session is required; never silently skip the key branch.
    LOKA_VERIFY([native isKeyWindow]);
    Scene &scene = *window->scene();
    Node *root = SceneAccess::rootNode(scene);
    loka::dsl::CompositionCursor<Node> children(root->asNestable()->childrenHead(),
                                                root->asNestable()->childrenCount());
    Node *firstNode = children.next();
    Node *secondNode = children.next();
    Node *editorNode = children.next();
    NSTextField *first = field(firstNode);
    NSTextField *second = field(secondNode);
    NSTextView *textEditor = editor((NSView *)NativeAccess::contentView(*window), editorNode->getContext());
    LOKA_VERIFY(textEditor && editorNode && editorNode->getContext());
    MacScenePlatformController *rail =
        static_cast<MacScenePlatformController *>(SceneAccess::platformController(scene));
    NodeContext *read = 0;
    LOKA_VERIFY([native makeFirstResponder:first]);
    LOKA_VERIFY([first currentEditor] != nil);
    LOKA_VERIFY(rail->readNativeFocus(read) && read == firstNode->getContext());
    LOKA_VERIFY(!(facts.focus.state()->get() != Fact::none()));
    // A foreign field cannot acquire the participant mark by borrowing a delegate.
    NSTextField *foreignField = [[NSTextField alloc] initWithFrame:NSZeroRect];
    [foreignField setDelegate:[first delegate]];
    LOKA_VERIFY(MacEditTextContext::fromNativeFocus(foreignField) == 0);
    [foreignField setDelegate:nil];
    [foreignField release];
    id firstDelegate = [first delegate];
    [first setDelegate:(id)native];
    LOKA_VERIFY(MacEditTextContext::fromNativeFocus(first) == 0);
    [first setDelegate:firstDelegate];
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

    [native resignKeyWindow];
    LOKA_VERIFY(![native isKeyWindow]);
    LOKA_VERIFY([native makeFirstResponder:nil]);
    LOKA_VERIFY(!rail->readNativeFocus(read));
    app.flushInvalidationsTick();
    LOKA_VERIFY(!(facts.focus.state()->get() != Fact(2u)));
    [native makeKeyWindow];
    LOKA_VERIFY([native isKeyWindow]);
    app.flushInvalidationsTick();
    LOKA_VERIFY(!(facts.focus.state()->get() != Fact::none()));

    textEditor = editor((NSView *)NativeAccess::contentView(*window), editorNode->getContext());
    LOKA_VERIFY(textEditor && [native makeFirstResponder:textEditor]);
    id editorDelegate = [textEditor delegate];
    [textEditor setDelegate:(id)native];
    LOKA_VERIFY(MacTextEditorContext::fromNativeFocus(textEditor) == 0);
    [textEditor setDelegate:editorDelegate];
    LOKA_VERIFY(rail->readNativeFocus(read) && read == editorNode->getContext());
    app.flushInvalidationsTick();
    LOKA_VERIFY(!(facts.focus.state()->get() != Fact(3u)));

    // A foreign view carrying the genuine delegate still lacks the typed mark.
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
    first = field(firstNode);
    [first setFrame:NSMakeRect(10, 10, 160, 24)];
    const NSPoint point = [first convertPoint:NSMakePoint(8, 12) toView:nil];
    NSEvent *up = [NSEvent mouseEventWithType:NSLeftMouseUp
                                     location:point
                                modifierFlags:0
                                    timestamp:0
                                 windowNumber:[native windowNumber]
                                      context:nil
                                  eventNumber:1
                                   clickCount:1
                                     pressure:0];
    [NSApp postEvent:up atStart:YES];
    NSEvent *down = [NSEvent mouseEventWithType:NSLeftMouseDown
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

    // AppKit may outlive the logical contexts; retained native objects lose owners.
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
