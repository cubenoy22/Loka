#include "ToolboxInputDoor.hpp"
#include "support/TestVerify.hpp"
#include "testing/core/StateTrackerTestAccess.hpp"
#include "support/Headless.hpp"
#include "support/LifecycleFactTestAccess.hpp"
#include "support/WindowAdmissionTestApp.hpp"
#include "support/StandaloneMountTestSupport.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "testing/scene/SceneFocusTestAccess.hpp"
#include "toolbox/ToolboxTextEditorAccess.hpp"
#include "ToolboxBuiltInSupport.hpp"
#include <cstdio>
#include <cstring>
using namespace loka::app;
using namespace loka::app::scene;
using namespace loka::core;
enum FieldKey
{
  FIRST = 1,
  SECOND = 2
};
namespace loka
{
  namespace app
  {
    template <> struct FocusKeyTraits<FieldKey> : UnsignedFocusKeyTraits<FieldKey>
    {
    };
  } // namespace app
} // namespace loka
#include "ToolboxActivationPhase.hpp"
/** Host App neighbor; the completion body below is the production method. */
class ToolboxApp : public WindowAdmissionTestApp
{
public:
  explicit ToolboxApp(Window &window)
      : WindowAdmissionTestApp(window)
  {
  }
  void present(ActivationPhase phase, loka::core::Operation &turn);
};
#include "ToolboxPresent.cpp"
namespace
{
  class Root : public BoundaryNodeFor<Root>
  {
  public:
    explicit Root(const BoundaryPropsFor<Root> &p)
        : BoundaryNodeFor<Root>(p)
    {
    }
    virtual void composeNode(NodeComposition &) {}
  };
  class FocusWindow : public Window
  {
  public:
    FocusWindow(PlatformContext *context, ToolboxScenePlatformController &controller)
        : Window(context, props()),
          native_(controller.window_)
    {
      this->scene()->mount(&controller);
    }
    ~FocusWindow()
    {
      this->unmountSceneForTeardown(*this->scene());
    }
    static WindowProps props()
    {
      WindowProps p;
      p.scene(new Scene(Boundary<Root>()));
      return p;
    }
    virtual bool hasLiveScenePlatform() const
    {
      return true;
    }
    virtual ToolboxWindow *asToolboxWindow()
    {
      return this->native_;
    }

  private:
    ToolboxWindow *native_;
  };
  struct Fixture : HeadlessStateOwner
  {
    loka::app::Focus<FieldKey> focus;
    NodeState<String> text, replacement;
    ToolboxWindow nativeWindow;
    ToolboxScenePlatformController controller;
    loka::testing::StandaloneMountTestPlatformContext platform;
    FocusWindow window;
    ToolboxApp app;
    EditTextNode first, second;
    Fixture()
        : controller(&nativeWindow),
          window(&platform, controller),
          app(window),
          first(EditTextProps()),
          second(EditTextProps())
    {
      app.flush();
      StateBatchBase::CreateImmediateState(loka::dsl::testing::SceneTestAccess::rootBoundary(*window.scene()), focus);
      StateBatchBase::CreateImmediateState(this, text, String::Literal("a"));
      StateBatchBase::CreateImmediateState(this, replacement, String::Literal("b"));
      first.props = EditTextProps(text).focusedAs(focus, FIRST);
      second.props = EditTextProps(text).focusedAs(focus, SECOND);
      app.flush();
      project(first);
      project(second);
    }
    ~Fixture()
    {
      retire(first);
      retire(second);
    }
    void attach(Node &node)
    {
      ComponentContext context;
      BoundaryNode *root = loka::dsl::testing::SceneTestAccess::rootBoundary(*window.scene());
      context.setBoundary(root);
      context.setStateOwner(root);
      context.setScene(window.scene());
      BoundaryNode::composeSubtree(&node, context, COMPOSE_EVENT_ATTACH, root);
    }
    void project(EditTextNode &node)
    {
      attach(node);
      node.setContext(new ToolboxEditTextContext(&node));
    }
    ToolboxEditTextContext *context(EditTextNode &node)
    {
      return static_cast<ToolboxEditTextContext *>(node.getContext());
    }
    Rect rect(int x = 0)
    {
      Rect r;
      SetRect(&r, x, 0, x + 40, 20);
      return r;
    }
    void hit(EditTextNode &node, int x = 0)
    {
      controller.recordEditHit(rect(x), context(node)->projectedTextState(), 0, context(node));
    }
    void native(EditTextNode &node, int x = 0)
    {
      LOKA_VERIFY(controller.ensureEditTextControl(
          context(node), rect(x), context(node)->projectedTextState(), NATIVE_HINT_DEFAULT));
    }
    void click(int x = 0)
    {
      Point p = {5, static_cast<short>(x + 5)};
      LOKA_VERIFY(ToolboxInputDoor::mouseDown(controller, p));
    }
    void expect(EditTextNode *node)
    {
      NodeContext *out = 0;
      LOKA_VERIFY(controller.readNativeFocus(out));
      LOKA_VERIFY(out == (node ? node->getContext() : 0));
      {
        loka::core::Operation turn;
        app.present(ACTIVATION_FOREGROUND, turn);
        LOKA_VERIFY(!loka::core::testing::OperationTestAccess::active());
      }
      LOKA_VERIFY(!loka::core::testing::OperationTestAccess::active());
      LOKA_VERIFY(!loka::core::Operation::isSettling());
      const Focused<FieldKey> value = focus.state()->get();
      LOKA_VERIFY(!(value != (node ? Focused<FieldKey>(node == &first ? FIRST : SECOND) : Focused<FieldKey>::none())));
    }
    void retire(EditTextNode &node)
    {
      LifecycleFactTestAccess::MarkSubtreeRetired(&node);
      controller.retireEditTextControl(node.getContext(), NATIVE_HINT_EAGER_RELEASE);
      node.setContext(0);
    }
  };
  void postedNative()
  {
    Fixture f;
    f.native(f.first);
    f.native(f.second, 80);
    f.click();
    f.expect(&f.first);
    TEHandle oldTE = f.controller.editControls_[0].te;
    TEHandle target = f.controller.editControls_[1].te;
    TESetSelect(0, 1, target);
    TEScroll(0, -7, target);
    const Rect dest = (**target).destRect;
    const int selections = toolbox_host::selections;
    GrafPtr before = 0;
    GetPort(&before);
    GrafPort ambient = {7, 12, 0};
    SetPort(&ambient);
    f.focus.post(SECOND);
    {
      loka::core::Operation turn;
      f.app.present(ACTIVATION_FOREGROUND, turn);
      LOKA_VERIFY(!loka::core::testing::OperationTestAccess::active());
    }
    LOKA_VERIFY(!loka::core::testing::OperationTestAccess::active());
    LOKA_VERIFY(!loka::core::Operation::isSettling());
    LOKA_VERIFY(!(**oldTE).active && (**target).active);
    LOKA_VERIFY(f.controller.editControls_.focused() == &f.controller.editControls_[1]);
    LOKA_VERIFY((**target).selStart == 0 && (**target).selEnd == 1);
    LOKA_VERIFY(EqualRect(&dest, &(**target).destRect));
    LOKA_VERIFY(toolbox_host::selections == selections);
    LOKA_VERIFY(f.text.get().equals(String::Literal("a")));
    GrafPtr after = 0;
    GetPort(&after);
    LOKA_VERIFY(after == &ambient);
    SetPort(before);
    LOKA_VERIFY(toolbox_host::activationPort == f.nativeWindow.window());
    LOKA_VERIFY(toolbox_host::deactivationPort == f.nativeWindow.window());
    f.expect(&f.second);
    f.click();
    f.expect(&f.first);
    LOKA_VERIFY((**oldTE).active && !(**target).active);
  }
  void postedEditor()
  {
    Fixture f;
    ObservableList<String> lines;
    Reported<LineCursor> cursor;
    StateBatchBase::CreateImmediateState(&f, cursor, LineCursor::None());
    LOKA_VERIFY(lines.attach(f.tracker()->asPushTracker(), 16) == ATTACH_OK);
    for (int i = 0; i < 10; ++i)
      LOKA_VERIFY(lines.insert(i, String::Literal("abcdef")) == EDIT_OK);
    TextEditorNode editor(TextEditorProps(lines, cursor).focusedAs(f.focus, SECOND));
    // Use the editor as the second key, with no duplicate attached binding.
    f.retire(f.second);
    f.attach(editor);
    LayoutState layout;
    layout.x = 80; layout.y = 0; layout.width = 80; layout.height = 40;
    LOKA_VERIFY(RegisterToolboxBuiltInSupport(f.controller));
    IPlatformNodeHandler *handler = f.controller.nodeHandlerRegistry_.find(&editor);
    LOKA_VERIFY(handler);
    ToolboxTextEditorContext *context = static_cast<ToolboxTextEditorContext *>(
        handler->ensureContext(&editor, &f.controller, layout));
    LOKA_VERIFY(context);
    context->layout(&f.controller, layout);
    context->render(&f.controller);
    TEHandle te = loka::testing::ToolboxTextEditorAccess::te(*context);
    LOKA_VERIFY(te);
    TESetSelect(8, 10, te);
    TEScroll(0, -16, te);
    const Rect dest = (**te).destRect;
    const LineCursor before = cursor.state()->get();
    const ListRevision revision = lines.revision().get();
    f.focus.post(SECOND);
    {
      loka::core::Operation turn;
      f.app.present(ACTIVATION_FOREGROUND, turn);
      LOKA_VERIFY(!loka::core::testing::OperationTestAccess::active());
    }
    LOKA_VERIFY(!loka::core::testing::OperationTestAccess::active());
    LOKA_VERIFY(!loka::core::Operation::isSettling());
    LOKA_VERIFY(f.focus.state()->get().is(SECOND));
    LOKA_VERIFY((**te).active);
    LOKA_VERIFY((**te).selStart == 8 && (**te).selEnd == 10);
    LOKA_VERIFY(EqualRect(&dest, &(**te).destRect));
    LOKA_VERIFY(cursor.state()->get() == before);
    LOKA_VERIFY(lines.revision().get().content == revision.content);
    Point click = {5, 85};
    LOKA_VERIFY(ToolboxInputDoor::mouseDown(f.controller, click));
    LOKA_VERIFY((**te).selStart == (**te).selEnd);
    LifecycleFactTestAccess::MarkSubtreeRetired(&editor);
    context->onFactChanged(NODE_FACT_ATTACHED, NODE_FACT_RETIRED);
    editor.setContext(0);
  }
  void admissionGuards()
  {
    Fixture f;
    f.native(f.first);
    f.native(f.second, 80);
    f.click();
    f.expect(&f.first);
    toolbox_host::frontWindow = 0;
    LOKA_VERIFY(!f.controller.applyNativeFocus(*f.context(f.second)));
    toolbox_host::frontWindow = f.nativeWindow.window();
    f.controller.editControls_[1].usedThisFrame = false;
    LOKA_VERIFY(!f.controller.applyNativeFocus(*f.context(f.second)));
    f.controller.editControls_[1].usedThisFrame = true;
    const Rect rect = f.controller.editControls_[1].rect;
    SetRect(&f.controller.editControls_[1].rect, 0, 0, 0, 0);
    LOKA_VERIFY(!f.controller.applyNativeFocus(*f.context(f.second)));
    f.controller.editControls_[1].rect = rect;
    const TEHandle te = f.controller.editControls_[1].te;
    f.controller.editControls_[1].te = 0;
    LOKA_VERIFY(!f.controller.applyNativeFocus(*f.context(f.second)));
    f.controller.editControls_[1].te = te;
    ToolboxEditTextContext stale(&f.second);
    LOKA_VERIFY(!f.controller.applyNativeFocus(stale));
    ToolboxScenePlatformController foreign(&f.nativeWindow);
    LOKA_VERIFY(!foreign.applyNativeFocus(*f.context(f.second)));
    f.expect(&f.first);
    LifecycleFactTestAccess::MarkSubtreeRetired(&f.second);
    LOKA_VERIFY(!f.controller.applyNativeFocus(*f.context(f.second)));
  }
  void postedFallback()
  {
    Fixture f;
    f.second.props.text(f.replacement);
    f.native(f.first);
    f.hit(f.second, 80);
    f.click();
    f.expect(&f.first);
    TEHandle native = f.controller.editControls_[0].te;
    f.focus.post(SECOND);
    {
      loka::core::Operation turn;
      f.app.present(ACTIVATION_FOREGROUND, turn);
      LOKA_VERIFY(!loka::core::testing::OperationTestAccess::active());
    }
    LOKA_VERIFY(!loka::core::testing::OperationTestAccess::active());
    LOKA_VERIFY(!loka::core::Operation::isSettling());
    LOKA_VERIFY(!(**native).active);
    LOKA_VERIFY(!f.controller.editControls_.focused());
    f.expect(&f.second);
    LOKA_VERIFY(ToolboxInputDoor::keyDown(f.controller, 'x'));
    LOKA_VERIFY(f.replacement.get().equals(String::Literal("bx")));
    f.focus.post(FIRST);
    {
      loka::core::Operation turn;
      f.app.present(ACTIVATION_FOREGROUND, turn);
      LOKA_VERIFY(!loka::core::testing::OperationTestAccess::active());
    }
    LOKA_VERIFY(!loka::core::testing::OperationTestAccess::active());
    LOKA_VERIFY(!loka::core::Operation::isSettling());
    f.expect(&f.first);
    f.controller.retireEditTextControlAt(0, NATIVE_HINT_EAGER_RELEASE);
    // Retiring the native destination must not resurrect the old fallback.
    LOKA_VERIFY(!ToolboxInputDoor::keyDown(f.controller, 'y'));
    LOKA_VERIFY(f.replacement.get().equals(String::Literal("bx")));
  }
  void postedRefusal()
  {
    Fixture f;
    f.native(f.first);
    f.click();
    f.expect(&f.first);
    SetRect(&f.controller.projectionClip, 0, 0, 40, 20);
    LOKA_VERIFY(!f.controller.ensureEditTextControl(f.context(f.second), f.rect(80),
                                                  f.text.state(), NATIVE_HINT_DEFAULT));
    f.hit(f.second, 80);
    f.focus.post(SECOND);
    {
      loka::core::Operation turn;
      f.app.present(ACTIVATION_FOREGROUND, turn);
      LOKA_VERIFY(!loka::core::testing::OperationTestAccess::active());
    }
    LOKA_VERIFY(!loka::core::testing::OperationTestAccess::active());
    LOKA_VERIFY(!loka::core::Operation::isSettling());
    f.expect(&f.first);
    // Materializing later cannot replay the refused, consumed request.
    SetRect(&f.controller.projectionClip, 0, 0, 200, 200);
    f.native(f.second, 80);
    {
      loka::core::Operation turn;
      f.app.present(ACTIVATION_FOREGROUND, turn);
      LOKA_VERIFY(!loka::core::testing::OperationTestAccess::active());
    }
    LOKA_VERIFY(!loka::core::testing::OperationTestAccess::active());
    LOKA_VERIFY(!loka::core::Operation::isSettling());
    f.expect(&f.first);
  }
  void renderClick(void *data)
  {
    Fixture *f = static_cast<Fixture *>(data);
    f->click(80);
  }
  void completion()
  {
    Fixture f;
    f.hit(f.first);
    f.hit(f.second, 80);
    f.click();
    f.expect(&f.first);
    f.nativeWindow.onFlush = &renderClick;
    f.nativeWindow.flushData = &f;
    {
      loka::core::Operation turn;
      f.app.present(ACTIVATION_BACKGROUND, turn);
      LOKA_VERIFY(!loka::core::testing::OperationTestAccess::active());
    }
    LOKA_VERIFY(!loka::core::testing::OperationTestAccess::active());
    LOKA_VERIFY(!loka::core::Operation::isSettling());
    LOKA_VERIFY(f.focus.state()->get().is(FIRST));
    {
      loka::core::Operation turn;
      f.app.present(ACTIVATION_FOREGROUND, turn);
      LOKA_VERIFY(!loka::core::testing::OperationTestAccess::active());
    }
    LOKA_VERIFY(!loka::core::testing::OperationTestAccess::active());
    LOKA_VERIFY(!loka::core::Operation::isSettling());
    LOKA_VERIFY(f.focus.state()->get().is(SECOND));
  }
  void endpointDeath()
  {
    Fixture f;
    {
      ToolboxScenePlatformController reader(&f.nativeWindow);
      reader.recordEditHit(f.rect(), f.text.state(), 0, f.context(f.first));
      Point p = {5, 5};
      LOKA_VERIFY(ToolboxInputDoor::mouseDown(reader, p));
      LOKA_VERIFY(SceneFocusTestAccess::sourced(*f.first.asFocusParticipant()));
    }
    LOKA_VERIFY(!SceneFocusTestAccess::sourced(*f.first.asFocusParticipant()));
    EditTextNode *field = new EditTextNode(EditTextProps(f.text));
    f.project(*field);
    f.hit(*field);
    f.click();
    delete field;
    f.expect(0);
    LOKA_VERIFY(!ToolboxInputDoor::keyDown(f.controller, 'x'));
  }
  void nativeFocus()
  {
    Fixture f;
    f.native(f.first);
    f.click();
    f.expect(&f.first);
    LOKA_VERIFY(ToolboxInputDoor::keyDown(f.controller, 'x'));
    LOKA_VERIFY(f.text.get().equals(String::Literal("xa")));
    f.controller.editControls_.clearFocus();
    f.expect(0);
    f.click();
    f.controller.retireEditTextBinding(f.controller.editControls_[0], NATIVE_HINT_EAGER_RELEASE);
    f.controller.editControls_.clear();
    f.expect(0);
    f.native(f.first);
    f.click();
    f.controller.retireEditTextControlAt(0, NATIVE_HINT_EAGER_RELEASE);
    f.expect(0);
    f.native(f.first);
    f.click();
    f.retire(f.first);
    f.expect(0);
    LOKA_VERIFY(!ToolboxInputDoor::keyDown(f.controller, 'x'));
  }
  void sharedTextKey()
  {
    Fixture f;
    f.hit(f.first);
    f.click();
    f.hit(f.second, 80);
    f.second.props.text(f.replacement);
    LOKA_VERIFY(ToolboxInputDoor::keyDown(f.controller, 'x'));
    LOKA_VERIFY(f.text.get().equals(String::Literal("ax")));
    LOKA_VERIFY(f.replacement.get().equals(String::Literal("b")));
  }
  void fallback()
  {
    Fixture f;
    toolbox_host::failNew = 1;
    LOKA_VERIFY(!f.controller.ensureEditTextControl(f.context(f.first), f.rect(), f.text.state(), NATIVE_HINT_DEFAULT));
    f.hit(f.first);
    f.click();
    f.expect(&f.first);
    f.expect(&f.first);
    toolbox_host::frontWindow = 0;
    NodeContext *out = 0;
    LOKA_VERIFY(!f.controller.readNativeFocus(out));
    f.app.reconcileFocus();
    LOKA_VERIFY(f.focus.state()->get().is(FIRST));
    toolbox_host::frontWindow = f.nativeWindow.window();
    // Reprojecting another field with the same text must not change identity.
    f.hit(f.second, 80);
    f.expect(&f.first);
    f.second.props.text(f.replacement);
    LOKA_VERIFY(ToolboxInputDoor::keyDown(f.controller, 'x'));
    LOKA_VERIFY(f.text.get().equals(String::Literal("ax")));
    LOKA_VERIFY(f.replacement.get().equals(String::Literal("b")));
    // Current props, not cached text state, are the key delivery source.
    f.first.props.text(f.replacement);
    LOKA_VERIFY(ToolboxInputDoor::keyDown(f.controller, 'y'));
    LOKA_VERIFY(f.replacement.get().equals(String::Literal("by")));
    // Moving the hit before retirement cannot leave a dangling text target.
    f.hit(f.first, 160);
    f.retire(f.first);
    f.expect(0);
    LOKA_VERIFY(!ToolboxInputDoor::keyDown(f.controller, 'z'));
  }
  void nativeWins()
  {
    Fixture f;
    f.hit(f.first);
    f.click();
    f.native(f.second, 80);
    f.click(80);
    f.expect(&f.second);
    f.controller.retireEditTextControlAt(0, NATIVE_HINT_EAGER_RELEASE);
    f.expect(0);
  }
  void promotion()
  {
    Fixture f;
    f.hit(f.first);
    f.click();
    f.native(f.first);
    f.expect(0);
    LOKA_VERIFY(!ToolboxInputDoor::keyDown(f.controller, 'x'));
  }
  void blankClick()
  {
    Fixture f;
    f.hit(f.first);
    f.click();
    f.expect(&f.first);
    Point outside = {1000, 1000};
    LOKA_VERIFY(!ToolboxInputDoor::mouseDown(f.controller, outside));
    f.expect(0);
    LOKA_VERIFY(!ToolboxInputDoor::keyDown(f.controller, 'x'));
  }
  void clipping()
  {
    Fixture f;
    f.hit(f.first);
    f.click();
    SetRect(&f.controller.projectionClip, 200, 200, 220, 220);
    f.hit(f.second, 80);
    f.expect(&f.first);
    f.hit(f.first);
    f.expect(0);
    LOKA_VERIFY(!ToolboxInputDoor::keyDown(f.controller, 'x'));
  }
} // namespace
int main(int argc, char **argv)
{
  if (argc == 2 && std::strcmp(argv[1], "shared-text-key") == 0)
  {
    sharedTextKey();
    return 0;
  }
  postedNative();
  admissionGuards();
  postedEditor();
  postedFallback();
  postedRefusal();
  sharedTextKey();
  nativeFocus();
  fallback();
  nativeWins();
  promotion();
  clipping();
  blankClick();
  completion();
  endpointDeath();
  std::puts("Toolbox focus host pins passed");
}
