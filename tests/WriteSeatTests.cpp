#include "platform/null/NullInputDoor.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/nestable/Show.hpp"
#include "platform/null/NullWindow.hpp"
#include "platform/null/NullPlatformContext.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#include "support/WindowAdmissionTestApp.hpp"
#include <cstdio>


#include "WriteSeatTests.hpp"
#include "support/TestVerify.hpp"
#include "app/scene/state/NodeState.hpp"
#include "app/nodes/controls/EditText.hpp"
#include "app/nodes/controls/PopupMenu.hpp"
#include "core/State.hpp"
#include "core/StateTracker.hpp"
#include "core/util/StateTrackerGuard.hpp"

using namespace loka::core;
using namespace loka::app;
using namespace loka::app::scene;
namespace { struct Count { int calls; Count() : calls(0) {} static void hit(void *p) { ++static_cast<Count *>(p)->calls; } }; }

void testWriteSeatSettlesIdleOwnerTracker()
{
  MutableState<int> value(0); PushStateTracker tracker; tracker.addState(&value);
  NodeState<int> state(&value, &tracker); Count count; value.bind(&Count::hit, &count, false);
  state.writeSeat().set(2, true);
  LOKA_VERIFY(value.get() == 2 && count.calls == 1 && tracker.phase() == TRACKER_IDLE);
}
void testWriteSeatJoinsOpenTracker()
{
  MutableState<int> value(0); PushStateTracker tracker; tracker.addState(&value);
  NodeState<int> state(&value, &tracker); Count count; value.bind(&Count::hit, &count, false);
  { StateTrackerGuard guard(&tracker); state.writeSeat().set(2, true); LOKA_VERIFY(count.calls == 1); }
  LOKA_VERIFY(count.calls == 1 && tracker.phase() == TRACKER_IDLE);
}
void testWriteSeatRawAndInvalid()
{
  MutableState<int> value(0); Count count; value.bind(&Count::hit, &count, false);
  WriteSeat<int>(&value).set(1, true); LOKA_VERIFY(value.get() == 1 && count.calls == 1);
  WriteSeat<int> invalid; invalid.set(2, true); LOKA_VERIFY(value.get() == 1);
}
void testWriteSeatPropsIdentity()
{
  MutableState<int> value(0); PushStateTracker a; PushStateTracker b; NodeState<int> left(&value, &a), right(&value, &b);
  PopupMenuProps one; one.selectedIndex(left); PopupMenuProps two; two.selectedIndex(right);
  LOKA_VERIFY(!(one < two) && !(two < one));
}

namespace
{
  struct Input977Ledger
  {
    unsigned sequence, destroyed, returned, emitted, applies;
    Input977Ledger() : sequence(0), destroyed(0), returned(0), emitted(0), applies(0) {}
  };
  Input977Ledger *input977Ledger = 0;
  bool input977ParentShown = false;
  class Input977Root;
  class Input977Child;
  Input977Root *input977Root = 0;
  Input977Child *input977Child = 0;

  class Input977Context : public NullScrollBarContext
  {
  public:
    Input977Context(ScrollBarNode &node, NullScenePlatformController &platform)
        : NullScrollBarContext(&node, &platform) {}
    virtual ~Input977Context()
    {
      input977Ledger->destroyed = ++input977Ledger->sequence;
    }
  };
  class Input977Root : public BoundaryNodeFor<Input977Root>
  {
  public:
    NodeState<bool> shown;
    explicit Input977Root(const BoundaryPropsFor<Input977Root> &p) : BoundaryNodeFor<Input977Root>(p)
    {
      input977Root = this;
      this->state(this->shown, true);
    }
    virtual void composeNode(NodeComposition &c);
  };
  class Input977Child : public BoundaryNodeFor<Input977Child>
  {
  public:
    NodeState<int> value;
    EmitterState changed;
    NodeState<bool> shown;
    explicit Input977Child(const BoundaryPropsFor<Input977Child> &p) : BoundaryNodeFor<Input977Child>(p)
    {
      input977Child = this;
      this->state(this->value, 0);
      this->state(this->shown, true);
    }
    virtual void composeNode(NodeComposition &c)
    {
      c.declare(Show(*(input977ParentShown ? input977Root->shown.state() : this->shown.state()))
                    .destroyOnDetach() << ScrollBar(this->value).range(0, 10).onChange(&this->changed));
    }
  };
  void Input977Root::composeNode(NodeComposition &c) { c.declare(Boundary<Input977Child>()); }
  ScrollBarNode *input977Find(Node *node)
  {
    if (node->nodeTypeKey() == NodeTypeToken<ScrollBarNode>())
      return static_cast<ScrollBarNode *>(node);
    INestable *children = node->asNestable();
    for (Node *child = children ? children->childrenHead() : 0; child; child = child->nextInComposition)
    {
      ScrollBarNode *found = input977Find(child);
      if (found) return found;
    }
    return 0;
  }
  class Input977Presenter : public NullScenePlatformController
  {
  public:
    virtual bool canSkipGlobalChangeForBoundaryLocalPaint() const { return false; }
    virtual void onChange(Node *, NodeDirtyFlags, bool) { ++input977Ledger->applies; }
  };
  void input977Emitted(void *)
  {
    LOKA_VERIFY(input977Child->value.get() == 1);
    LOKA_VERIFY(input977Ledger->destroyed == 0);
    input977Ledger->emitted = ++input977Ledger->sequence;
  }
  struct Input977Observer
  {
    NodeState<bool> &shown;
    unsigned calls;
    Input977Observer(NodeState<bool> &s) : shown(s), calls(0)
    { input977Child->value.state()->bind(&changed, this, false); }
    ~Input977Observer() { input977Child->value.state()->unbind(&changed, this); }
    static void changed(void *data)
    {
      Input977Observer &self = *static_cast<Input977Observer *>(data);
      ++self.calls;
      const bool values[] = {false, true, false};
      for (unsigned i = 0; i < 3; ++i)
      {
        self.shown.set(values[i]);
      }
    }
  };
}

void testNullInputDoorLifetime()
{
  for (unsigned arrangement = 0; arrangement < 2; ++arrangement)
  {
    Input977Ledger ledger;
    input977Ledger = &ledger;
    input977ParentShown = arrangement != 0;
    NullPlatformContext context;
    Input977Presenter platform;
    WindowProps props;
    props.scene(new Scene(Boundary<Input977Root>()));
    NullWindow window(&context, props, &platform);
    WindowAdmissionTestApp app(window);
    app.flush();
    Scene &scene = *window.scene();
    ScrollBarNode *node = input977Find(loka::dsl::testing::SceneTestAccess::rootBoundary(scene));
    LOKA_VERIFY(node);
    Input977Context *input = new Input977Context(*node, platform);
    node->setContext(input);
    input->readLifecycleFactOnAttach();
    NodeState<bool> &shown = arrangement ? input977Root->shown : input977Child->shown;
    Input977Observer observer(shown);
    input977Child->changed.bind(&input977Emitted, 0, false);
    ledger.applies = 0;
    NullInputDoor::simulateThumbDragTo(*input, 1);
    ledger.returned = ++ledger.sequence;
    std::fprintf(stderr, "[977] arrangement=%u returned=%u destroyed=%u applies=%u\n",
                 arrangement, ledger.returned, ledger.destroyed, ledger.applies);
    LOKA_VERIFY(observer.calls == 1 && input977Child->value.get() == 1);
    LOKA_VERIFY(ledger.destroyed == 0);
    LOKA_VERIFY(ledger.emitted != 0 && ledger.returned > ledger.emitted);
    LOKA_VERIFY(ledger.applies == 0);
    LOKA_VERIFY(!platform.borrowPhase().open());
    // Returning through the door never pumps: even the final false waits.
    LOKA_VERIFY(input977Find(loka::dsl::testing::SceneTestAccess::rootBoundary(scene)) == node);
    app.flush();
    LOKA_VERIFY(ledger.destroyed > ledger.returned && ledger.applies > 0);
    LOKA_VERIFY(!input977Find(loka::dsl::testing::SceneTestAccess::rootBoundary(scene)));
    input977Child->changed.unbind(&input977Emitted, 0);
  }
  input977Ledger = 0;
  input977Root = 0;
  input977Child = 0;
}
