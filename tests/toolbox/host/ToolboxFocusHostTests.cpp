#include "support/TestVerify.hpp"
#include "support/Headless.hpp"
#include "support/LifecycleFactTestAccess.hpp"
#include "support/WindowAdmissionTestApp.hpp"
#include "support/StandaloneMountTestSupport.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "testing/scene/SceneFocusTestAccess.hpp"
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
  void present(ActivationPhase phase);
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
    Reported<Focused<FieldKey> > focus;
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
      StateBatchBase::CreateImmediateState(this, focus, Focused<FieldKey>::none());
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
    void project(EditTextNode &node)
    {
      ComponentContext context;
      BoundaryNode *root = loka::dsl::testing::SceneTestAccess::rootBoundary(*window.scene());
      context.setBoundary(root);
      context.setStateOwner(root);
      context.setScene(window.scene());
      BoundaryNode::composeSubtree(&node, context, COMPOSE_EVENT_ATTACH, root);
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
      LOKA_VERIFY(controller.handleMouseDown(p));
    }
    void expect(EditTextNode *node)
    {
      NodeContext *out = 0;
      LOKA_VERIFY(controller.readNativeFocus(out));
      LOKA_VERIFY(out == (node ? node->getContext() : 0));
      app.present(ACTIVATION_FOREGROUND);
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
    f.app.present(ACTIVATION_BACKGROUND);
    LOKA_VERIFY(f.focus.state()->get().is(FIRST));
    f.app.present(ACTIVATION_FOREGROUND);
    LOKA_VERIFY(f.focus.state()->get().is(SECOND));
  }
  void endpointDeath()
  {
    Fixture f;
    {
      ToolboxScenePlatformController reader(&f.nativeWindow);
      reader.recordEditHit(f.rect(), f.text.state(), 0, f.context(f.first));
      Point p = {5, 5};
      LOKA_VERIFY(reader.handleMouseDown(p));
      LOKA_VERIFY(SceneFocusTestAccess::sourced(*f.first.asFocusParticipant()));
    }
    LOKA_VERIFY(!SceneFocusTestAccess::sourced(*f.first.asFocusParticipant()));
    EditTextNode *field = new EditTextNode(EditTextProps(f.text));
    f.project(*field);
    f.hit(*field);
    f.click();
    delete field;
    f.expect(0);
    LOKA_VERIFY(!f.controller.handleKeyDown('x'));
  }
  void nativeFocus()
  {
    Fixture f;
    f.native(f.first);
    f.click();
    f.expect(&f.first);
    LOKA_VERIFY(f.controller.handleKeyDown('x'));
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
    LOKA_VERIFY(!f.controller.handleKeyDown('x'));
  }
  void sharedTextKey()
  {
    Fixture f;
    f.hit(f.first);
    f.click();
    f.hit(f.second, 80);
    f.second.props.text(f.replacement);
    LOKA_VERIFY(f.controller.handleKeyDown('x'));
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
    LOKA_VERIFY(f.controller.handleKeyDown('x'));
    LOKA_VERIFY(f.text.get().equals(String::Literal("ax")));
    LOKA_VERIFY(f.replacement.get().equals(String::Literal("b")));
    // Current props, not cached text state, are the key delivery source.
    f.first.props.text(f.replacement);
    LOKA_VERIFY(f.controller.handleKeyDown('y'));
    LOKA_VERIFY(f.replacement.get().equals(String::Literal("by")));
    // Moving the hit before retirement cannot leave a dangling text target.
    f.hit(f.first, 160);
    f.retire(f.first);
    f.expect(0);
    LOKA_VERIFY(!f.controller.handleKeyDown('z'));
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
    LOKA_VERIFY(!f.controller.handleKeyDown('x'));
  }
  void blankClick()
  {
    Fixture f;
    f.hit(f.first);
    f.click();
    f.expect(&f.first);
    Point outside = {1000, 1000};
    LOKA_VERIFY(!f.controller.handleMouseDown(outside));
    f.expect(0);
    LOKA_VERIFY(!f.controller.handleKeyDown('x'));
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
    LOKA_VERIFY(!f.controller.handleKeyDown('x'));
  }
} // namespace
int main(int argc, char **argv)
{
  if (argc == 2 && std::strcmp(argv[1], "shared-text-key") == 0)
  {
    sharedTextKey();
    return 0;
  }
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
