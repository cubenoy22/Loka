#include "NodeObservedUsesTests.hpp"
#include "support/TestVerify.hpp"
#include "support/ObservedUseCounts.hpp"
#include "support/LifecycleFactTestAccess.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#include "testing/scene/NodeObservedUsesTestAccess.hpp"
#include "app/nodes/Text.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/nestable/Show.hpp"
#include "app/nodes/nestable/Keyed.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/scene/node/ComponentNode.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "core/util/StateTrackerGuard.hpp"

namespace
{
  using namespace loka::app;
  using namespace loka::app::scene;
  using namespace loka::core;
  using loka::app::testing::NodeObservedUsesTestAccess;
  using loka::dsl::testing::SceneTestAccess;

  const NodeDirtyFlags both = static_cast<NodeDirtyFlags>(NODE_DIRTY_PROPS | NODE_DIRTY_LAYOUT);

  class EmptyBoundary : public BoundaryNodeFor<EmptyBoundary>
  {
  public:
    explicit EmptyBoundary(const BoundaryPropsFor<EmptyBoundary> &props = BoundaryPropsFor<EmptyBoundary>())
        : BoundaryNodeFor<EmptyBoundary>(props) {}
    virtual void composeNode(NodeComposition &) {}
  };

  class InputContext : public NodeContext
  {
  public:
    InputContext() : seen(NODE_DIRTY_NONE) {}
    virtual short layout(IPlatformController *, LayoutState &state)
    {
      this->seen = state.inputs;
      return 17;
    }
    NodeDirtyFlags seen;
  };

  void consumeMark(Node &node)
  {
    node.setContext(new InputContext());
    LayoutState state;
    node.layout(0, state);
    node.setContext(0);
  }

  struct LifecycleTrace
  {
    LifecycleTrace() : tracker(0), source(0), retired(0), attaches(0), attachMark(NODE_DIRTY_NONE) {}
    StateTracker *tracker;
    MutableState<String> *source;
    unsigned retired;
    unsigned attaches;
    NodeDirtyFlags attachMark;
  };
  LifecycleTrace *activeTrace = 0;

  class TracedText : public TextNode
  {
  public:
    explicit TracedText(const TextProps &props) : TextNode(props) {}
    virtual void onLifecycleFactChanged(NodeLifecycleFact, NodeLifecycleFact next)
    {
      LOKA_VERIFY(activeTrace != 0);
      if (next == NODE_FACT_ATTACHED)
      {
        ++activeTrace->attaches;
        activeTrace->attachMark = NodeObservedUsesTestAccess::mark(*this);
      }
      if (next == NODE_FACT_RETIRED && activeTrace->source)
      {
        ++activeTrace->retired;
        LOKA_VERIFY(NodeObservedUsesTestAccess::useCount(*this) == 0);
        StateTrackerGuard guard(activeTrace->tracker);
        activeTrace->source->set(String::Literal("retirement hook write"));
      }
    }
  };

  class RetainedTextComponent;
  struct RetainedTextComponentTag {};
  struct RetainedTextComponentProps : NodePropsBase<RetainedTextComponentProps>
  {
    typedef RetainedTextComponentTag TypeTag;
    typedef RetainedTextComponent NodeType;
    explicit RetainedTextComponentProps(State<String> *value = 0) : source(value) {}
    bool operator<(const PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != this->propsTypeId()) return false;
      return this->source < static_cast<const RetainedTextComponentProps &>(rhs).source;
    }
    State<String> *source;
  };
  class RetainedTextComponent : public ComponentNodeWithProps<RetainedTextComponentProps>
  {
  public:
    explicit RetainedTextComponent(const RetainedTextComponentProps &props)
        : ComponentNodeWithProps<RetainedTextComponentProps>(props) {}
    virtual void composeChildren(NodeComposition &composition)
    {
      composition.declare(NodeDefinition<TextProps, TracedText>(TextProps(this->props.source)));
    }
  };

  template <bool Destroy> class LifecycleRoot : public BoundaryNodeFor<LifecycleRoot<Destroy> >
  {
  public:
    NodeState<bool> shown;
    NodeState<String> text;
    explicit LifecycleRoot(const BoundaryPropsFor<LifecycleRoot<Destroy> > &props)
        : BoundaryNodeFor<LifecycleRoot<Destroy> >(props)
    {
      this->state(this->shown, true);
      this->state(this->text, String::Literal("before"));
    }
    virtual bool flushViewDirtyImmediately(NodeDirtyFlags) const { return false; }
    virtual void composeNode(NodeComposition &composition)
    {
      if (Destroy)
        composition.declare(Show(*this->shown.state()).destroyOnDetach() <<
          NodeDefinition<TextProps, TracedText>(TextProps(this->text.state())));
      else
        composition.declare(Show(*this->shown.state()) <<
          NodeDefinition<RetainedTextComponentProps, RetainedTextComponent>(
              RetainedTextComponentProps(this->text.state())));
    }
  };

  Node *findText(Node *node)
  {
    if (!node) return 0;
    if (node->nodeTypeKey() == NodeTypeToken<TextNode>()) return node;
    INestable *nestable = node->asNestable();
    for (Node *child = nestable ? nestable->childrenHead() : 0; child; child = child->nextInComposition)
    {
      Node *found = findText(child);
      if (found) return found;
    }
    return 0;
  }
}

void testObservedUsesSharedStateMarksBothNodes()
{
  std::printf("Observed-use sizeof host: Node=%lu ObservedUses=%lu ObservedUse=%lu LayoutState=%lu entry=%lu binding=%lu\n",
      static_cast<unsigned long>(sizeof(Node)), static_cast<unsigned long>(sizeof(ObservedUses)),
      static_cast<unsigned long>(sizeof(ObservedUse)), static_cast<unsigned long>(sizeof(LayoutState)),
      static_cast<unsigned long>(sizeof(BoundaryObservedStateEntry)),
      static_cast<unsigned long>(sizeof(BoundaryObservedStateBinding)));
  MutableState<String> source(String::Literal("before"));
  PushStateTracker tracker;
  tracker.addState(&source);
  TextNode first((TextProps(&source))), second((TextProps(&source)));
  EmptyBoundary owner;
  BoundaryNode::declareBoundaryDirtySources(&first, &owner);
  const NodeDirtyFlags singleUnion = owner.observedDirtyFlags();
  BoundaryNode::declareBoundaryDirtySources(&second, &owner);
  LOKA_VERIFY(owner.observedDirtyFlags() == singleUnion);
  LOKA_VERIFY(singleUnion != NODE_DIRTY_NONE);
  LOKA_VERIFY(NodeObservedUsesTestAccess::useCount(first) == 1);
  LOKA_VERIFY(NodeObservedUsesTestAccess::useCount(second) == 1);
  { StateTrackerGuard guard(&tracker); source.set(String::Literal("after")); }
  LOKA_VERIFY(NodeObservedUsesTestAccess::mark(first) == singleUnion);
  LOKA_VERIFY(NodeObservedUsesTestAccess::mark(second) == singleUnion);
  LOKA_VERIFY(owner.observedDirtyFlags() == singleUnion);
  LifecycleFactTestAccess::MarkSubtreeRetired(&first);
  LOKA_VERIFY(NodeObservedUsesTestAccess::useCount(first) == 0);
  LOKA_VERIFY(owner.observedDirtyFlags() == singleUnion);
  consumeMark(second);
  { StateTrackerGuard guard(&tracker); source.set(String::Literal("survivor")); }
  LOKA_VERIFY(NodeObservedUsesTestAccess::mark(second) == singleUnion);
  owner.clearObservedStateEntries();
  owner.clearObservedDirtyFlags();
  TextNode propsOnly((TextProps(&source))), layoutOnly((TextProps(&source)));
  LOKA_VERIFY(owner.registerObservedState(&source, NODE_DIRTY_PROPS, &propsOnly));
  LOKA_VERIFY(owner.registerObservedState(&source, NODE_DIRTY_LAYOUT, &layoutOnly));
  LOKA_VERIFY(owner.observedDirtyFlags() == both);
  { StateTrackerGuard guard(&tracker); source.set(String::Literal("mixed flags")); }
  LOKA_VERIFY(NodeObservedUsesTestAccess::mark(propsOnly) == NODE_DIRTY_PROPS);
  LOKA_VERIFY(NodeObservedUsesTestAccess::mark(layoutOnly) == NODE_DIRTY_LAYOUT);
  LOKA_VERIFY(owner.observedDirtyFlags() == both);
  owner.clearObservedStateEntries();
}

void testObservedUsesRetiredBorrowerWithdrawsBeforeCommit()
{
  MutableState<String> source(String::Literal("before"));
  PushStateTracker tracker;
  tracker.addState(&source);
  EmptyBoundary owner;
  TextNode *leaf = new TextNode(TextProps(&source));
  BoundaryNode::declareBoundaryDirtySources(leaf, &owner);
  LOKA_VERIFY(NodeObservedUsesTestAccess::useCount(*leaf) == 1);
  LifecycleFactTestAccess::MarkSubtreeRetired(leaf);
  LOKA_VERIFY(NodeObservedUsesTestAccess::useCount(*leaf) == 0);
  { StateTrackerGuard guard(&tracker); source.set(String::Literal("retired, alive")); }
  delete leaf;
  // Repeat after reclaim: ASan also checks that no callback borrows freed storage.
  { StateTrackerGuard guard(&tracker); source.set(String::Literal("reclaimed")); }
  owner.clearObservedStateEntries();
}

void testObservedUsesSceneRetireHookWithdrawsBeforeWrite()
{
  LifecycleTrace trace;
  activeTrace = &trace;
  NullScenePlatformController platform;
  Scene scene((Boundary<LifecycleRoot<true> >()));
  scene.mount(&platform);
  SceneTestAccess::updateAttached(scene, true);
  LifecycleRoot<true> *root = static_cast<LifecycleRoot<true> *>(SceneTestAccess::rootBoundary(scene));
  LOKA_VERIFY(findText(root) != 0);
  trace.tracker = root->tracker();
  trace.source = static_cast<MutableState<String> *>(root->text.state());
  { StateTrackerGuard guard(root->tracker()); root->shown.set(false); }
  scene.flushInvalidation();
  LOKA_VERIFY(trace.retired == 1);
  LOKA_VERIFY(root->text.get().equals(String::Literal("retirement hook write")));
  trace.source = 0;
  SceneTestAccess::unmount(scene);
  activeTrace = 0;
}

void testObservedUsesParkedWriteAndReattach()
{
  // Construction already reports ATTACHED; the first attach application still
  // marks even though it does not produce a fact-change callback.
  TextNode initial((TextProps("initial")));
  LOKA_VERIFY(NodeObservedUsesTestAccess::mark(initial) == NODE_DIRTY_NONE);
  NotifySubtreeNodeAttached(&initial);
  LOKA_VERIFY(NodeObservedUsesTestAccess::mark(initial) == both);
  LifecycleTrace trace;
  activeTrace = &trace;
  NullScenePlatformController platform;
  Scene scene((Boundary<LifecycleRoot<false> >()));
  scene.mount(&platform);
  SceneTestAccess::updateAttached(scene, true);
  LifecycleRoot<false> *root = static_cast<LifecycleRoot<false> *>(SceneTestAccess::rootBoundary(scene));
  Node *leaf = findText(root);
  LOKA_VERIFY(leaf != 0);
  consumeMark(*leaf);
  { StateTrackerGuard guard(root->tracker()); root->shown.set(false); }
  scene.flushInvalidation();
  LOKA_VERIFY(leaf->lifecycleFact() == NODE_FACT_DETACHED_RETAINED);
  LOKA_VERIFY(NodeObservedUsesTestAccess::useCount(*leaf) == 0);
  { StateTrackerGuard guard(root->tracker()); root->text.set(String::Literal("parked write")); }
  LOKA_VERIFY(NodeObservedUsesTestAccess::mark(*leaf) == NODE_DIRTY_NONE);
  const unsigned before = trace.attaches;
  { StateTrackerGuard guard(root->tracker()); root->shown.set(true); }
  scene.flushInvalidation();
  LOKA_VERIFY(findText(root) == leaf);
  LOKA_VERIFY(trace.attaches == before + 1);
  LOKA_VERIFY(trace.attachMark == both);
  LOKA_VERIFY(NodeObservedUsesTestAccess::useCount(*leaf) == 1);
  SceneTestAccess::unmount(scene);
  activeTrace = 0;
}

void testObservedUsesPropsSuccessAndFailure()
{
  TextNode leaf((TextProps("before")));
  leaf.setPropsTypeId(TextProps::staticTypeId());
  Text next("after");
  LOKA_VERIFY(NodeObservedUsesTestAccess::mark(leaf) == NODE_DIRTY_NONE);
  Button wrong("wrong type");
  LOKA_VERIFY(!wrong.applyPropsToNode(&leaf));
  LOKA_VERIFY(NodeObservedUsesTestAccess::mark(leaf) == NODE_DIRTY_NONE);
  LOKA_VERIFY(next.applyPropsToNode(&leaf));
  LOKA_VERIFY(NodeObservedUsesTestAccess::mark(leaf) == both);

  // This matches the props type but its production applier deliberately refuses:
  // runtime generation roots are transferred once, never props-reconciled.
  NodeDefinition<KeyedGenerationProps, KeyedGenerationRoot> generation((KeyedGenerationProps()));
  Node *root = generation.create();
  LOKA_VERIFY(root != 0);
  LOKA_VERIFY(generation.isCompatibleWithNode(root));
  LOKA_VERIFY(NodeObservedUsesTestAccess::mark(*root) == NODE_DIRTY_NONE);
  LOKA_VERIFY(!generation.applyPropsToNode(root));
  LOKA_VERIFY(NodeObservedUsesTestAccess::mark(*root) == NODE_DIRTY_NONE);
  DestroyHeapNode(root);
}

void testObservedUsesLayoutInputsAreNodeLocal()
{
  TextNode first((TextProps("first"))), sibling((TextProps("sibling")));
  first.setPropsTypeId(TextProps::staticTypeId());
  InputContext *firstContext = new InputContext();
  InputContext *siblingContext = new InputContext();
  first.setContext(firstContext);
  sibling.setContext(siblingContext);
  Text changed("changed");
  LOKA_VERIFY(changed.applyPropsToNode(&first));
  LayoutState shared;
  LOKA_VERIFY(shared.inputs == NODE_DIRTY_NONE);
  LOKA_VERIFY(first.layout(0, shared) == 17);
  LOKA_VERIFY(firstContext->seen == both);
  LOKA_VERIFY(shared.inputs == NODE_DIRTY_NONE);
  LOKA_VERIFY(NodeObservedUsesTestAccess::mark(first) == NODE_DIRTY_NONE);
  sibling.layout(0, shared);
  LOKA_VERIFY(siblingContext->seen == NODE_DIRTY_NONE);
  shared.inputs = NODE_DIRTY_PROPS;
  first.layout(0, shared);
  LOKA_VERIFY(firstContext->seen == NODE_DIRTY_NONE);
  LOKA_VERIFY(shared.inputs == NODE_DIRTY_PROPS);
}

void testObservedUsesFinishPassPrunesOneSharedBorrower()
{
  MutableState<String> source(String::Literal("before"));
  TextNode first((TextProps(&source))), second((TextProps(&source)));
  EmptyBoundary owner;
  owner.beginObservedStatePass();
  BoundaryNode::declareBoundaryDirtySources(&first, &owner);
  BoundaryNode::declareBoundaryDirtySources(&second, &owner);
  owner.completeObservedStatePass();
  const NodeDirtyFlags originalUnion = owner.observedDirtyFlags();
  LOKA_VERIFY(NodeObservedUsesTestAccess::useCount(first) == 1);
  LOKA_VERIFY(NodeObservedUsesTestAccess::useCount(second) == 1);
  // A following declaration walk omits the parked borrower while the sibling
  // still renews the shared subscription. Subscription-only pruning misses this.
  owner.beginObservedStatePass();
  BoundaryNode::declareBoundaryDirtySources(&second, &owner);
  owner.completeObservedStatePass();
  LOKA_VERIFY(NodeObservedUsesTestAccess::useCount(first) == 0);
  LOKA_VERIFY(NodeObservedUsesTestAccess::useCount(second) == 1);
  LOKA_VERIFY(owner.observedDirtyFlags() == originalUnion);
  owner.beginObservedStatePass();
  owner.completeObservedStatePass();
  LOKA_VERIFY(NodeObservedUsesTestAccess::useCount(second) == 0);
}

#include "support/LokaAllocFailure.hpp"
namespace
{
  State<String> *refusalSource = 0;
  class RefusalRoot : public BoundaryNodeFor<RefusalRoot>
  {
  public:
    explicit RefusalRoot(const BoundaryPropsFor<RefusalRoot> &props)
        : BoundaryNodeFor<RefusalRoot>(props) {}
    virtual void composeNode(NodeComposition &composition)
    {
      composition.declare(Row() << Text(refusalSource) << Text(refusalSource));
    }
  };
}

void testObservedUsesAllocationRefusesWholeCandidate()
{
  for (int failure = 1; failure <= 2; ++failure)
  {
    loka::core::testing::failLokaAllocRaw("Node", "ObservedUse", failure);
    {
      MutableState<String> source(String::Literal("borrowed"));
      refusalSource = &source;
      NullScenePlatformController platform;
      {
        Scene scene((Boundary<RefusalRoot>()));
        scene.mount(&platform);
        SceneTestAccess::updateAttached(scene, true);
        BoundaryNode *root = SceneTestAccess::rootBoundary(scene);
        LOKA_VERIFY(root != 0);
        LOKA_VERIFY(root->childrenHead() == 0);
        LOKA_VERIFY(NodeObservedUsesTestAccess::useCount(*root) == 0);
        SceneTestAccess::unmount(scene);
      }
      // The fail-count hook refuses once. A new mount is an explicit outside
      // retry, after the incomplete candidate has been discarded in full.
      {
        Scene retry((Boundary<RefusalRoot>()));
        retry.mount(&platform);
        SceneTestAccess::updateAttached(retry, true);
        BoundaryNode *root = SceneTestAccess::rootBoundary(retry);
        LOKA_VERIFY(root != 0);
        LOKA_VERIFY(findText(root) != 0);
        const unsigned restoredUses = ObservedUseTestSupport::activeUses(root);
        std::printf("ObservedUse refusal %d recovered uses=%u\n", failure, restoredUses);
        LOKA_VERIFY(restoredUses == 2);
        SceneTestAccess::unmount(retry);
      }
      refusalSource = 0;
    }
    LOKA_VERIFY(loka::core::testing::lokaAllocRawLive() == 0);
    loka::core::testing::allowLokaAllocRaw();
  }
}

namespace
{
  class TwoBorrowedSources : public Node
  {
  public:
    TwoBorrowedSources(StateBase &first, StateBase &second) : first_(first), second_(second) {}
    virtual void declareDirtySources(DirtySourceRegistrar &registrar)
    {
      registrar.markDirtyOnChange(&this->first_, NODE_DIRTY_PROPS);
      registrar.markDirtyOnChange(&this->second_, NODE_DIRTY_LAYOUT);
    }
  private:
    StateBase &first_;
    StateBase &second_;
  };
}

void testObservedUsesRegistrationRefusalPublishesNoPartialUses()
{
  loka::core::testing::failLokaAllocRaw("Node", "ObservedUse", 0);
  {
    MutableState<bool> previous(false), first(false), second(false);
    EmptyBoundary owner;
    TwoBorrowedSources node(first, second);
    owner.beginObservedStatePass();
    LOKA_VERIFY(owner.registerObservedState(&previous, NODE_DIRTY_PROPS, &node));
    owner.completeObservedStatePass();
    LOKA_VERIFY(NodeObservedUsesTestAccess::useCount(node) == 1);

    // Model a changed declaration. Its old observation stays subscribed until
    // pass completion; neither new source may publish if the second row refuses.
    loka::core::testing::failLokaAllocRaw("Node", "ObservedUse", 2);
    owner.beginComposeResult(COMPOSE_EVENT_UPDATE, NODE_DIRTY_PROPS);
    owner.beginObservedStatePass();
    BoundaryNode::declareBoundaryDirtySources(&node, &owner);
    LOKA_VERIFY(NodeObservedUsesTestAccess::useCount(node) == 1);
    owner.completeObservedStatePass();
    owner.completeComposeResult();
    LOKA_VERIFY(owner.composeResult().allocationFailed);
    LOKA_VERIFY(!owner.composeResult().composed);
    LOKA_VERIFY(NodeObservedUsesTestAccess::useCount(node) == 0);

    // Outside retry reuses prepared storage only after the failed pass closed.
    owner.beginComposeResult(COMPOSE_EVENT_UPDATE, NODE_DIRTY_PROPS);
    owner.beginObservedStatePass();
    BoundaryNode::declareBoundaryDirtySources(&node, &owner);
    owner.completeObservedStatePass();
    owner.completeComposeResult();
    LOKA_VERIFY(!owner.composeResult().allocationFailed);
    LOKA_VERIFY(NodeObservedUsesTestAccess::useCount(node) == 2);
    owner.clearObservedStateEntries();
  }
  LOKA_VERIFY(loka::core::testing::lokaAllocRawLive() == 0);
  loka::core::testing::allowLokaAllocRaw();
}

namespace
{
  class RepeatedSeatRefusalRoot : public BoundaryNodeFor<RepeatedSeatRefusalRoot>
  {
  public:
    NodeState<bool> shown;
    NodeState<bool> other;
    explicit RepeatedSeatRefusalRoot(const BoundaryPropsFor<RepeatedSeatRefusalRoot> &props)
        : BoundaryNodeFor<RepeatedSeatRefusalRoot>(props)
    {
      this->state(this->shown, true);
      this->state(this->other, false);
    }
    virtual void composeNode(NodeComposition &composition)
    {
      composition.declare(Row()
          << (Show(*this->shown.state()) << Text("seat child"))
          << (Show(*this->other.state()) << Text("hidden child")));
    }
  };
}

void testObservedUsesRepeatedSeatRefusal()
{
  loka::core::testing::failLokaAllocRaw("Node", "ObservedUse", 1);
  {
    NullScenePlatformController platform;
    Scene scene((Boundary<RepeatedSeatRefusalRoot>()));
    scene.mount(&platform);
    SceneTestAccess::updateAttached(scene, true);
    RepeatedSeatRefusalRoot *root =
        static_cast<RepeatedSeatRefusalRoot *>(SceneTestAccess::rootBoundary(scene));
    LOKA_VERIFY(root != 0);
    LOKA_VERIFY(root->childrenHead() == 0);
    LOKA_VERIFY(root->composeResult().allocationFailed);
    LOKA_VERIFY(NodeObservedUsesTestAccess::useCount(*root) == 0);

    // Retry on the same owner: observation-pass admission happens again before
    // declaration. Its refusal must remain recorded through compose completion.
    loka::core::testing::failLokaAllocRaw("Node", "ObservedUse", 1);
    scene.requestInvalidate(NODE_DIRTY_CHILD);
    scene.flushInvalidation();
    LOKA_VERIFY(root->childrenHead() == 0);
    LOKA_VERIFY(root->composeResult().allocationFailed);
    LOKA_VERIFY(NodeObservedUsesTestAccess::useCount(*root) == 0);

    loka::core::testing::failLokaAllocRaw("Node", "ObservedUse", 0);
    scene.requestInvalidate(NODE_DIRTY_CHILD);
    scene.flushInvalidation();
    Node *leaf = findText(root);
    LOKA_VERIFY(leaf != 0);
    LOKA_VERIFY(!root->composeResult().allocationFailed);
    LOKA_VERIFY(NodeObservedUsesTestAccess::useCount(*root) == 2);
    { StateTrackerGuard guard(root->tracker()); root->shown.set(false); }
    scene.flushInvalidation();
    LOKA_VERIFY(findText(root) == 0);
    { StateTrackerGuard guard(root->tracker()); root->shown.set(true); }
    scene.flushInvalidation();
    LOKA_VERIFY(findText(root) == leaf);
    SceneTestAccess::unmount(scene);
  }
  LOKA_VERIFY(loka::core::testing::lokaAllocRawLive() == 0);
  loka::core::testing::allowLokaAllocRaw();
}
