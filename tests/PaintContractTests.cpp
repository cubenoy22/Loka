#include "PaintContractTests.hpp"
#include "../example/FloppyBird/src/MainNode.hpp"
#include "../example/SmirkBench/src/MainNode.hpp"
#include "app/scene/Scene.hpp"
#include "app/scene/projection/PaintEnumeration.hpp"
#include "app/scene/projection/PlatformNodeHandler.hpp"
#include "app/nodes/ImageView.hpp"
#include "app/nodes/nestable/ScrollView.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "platform/null/context/NullRectSurfaceContext.hpp"
#include "platform/null/context/NullTextContext.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "platform/String.hpp"
#include "support/RecomposingBoundary.hpp"
#include "support/LifecycleFactTestAccess.hpp"
#include "support/TestVerify.hpp"
#include "testing/scene/SceneTestFlow.hpp"

namespace
{
  using namespace loka::app;
  using namespace loka::app::scene;
  using namespace loka::core;
  using loka::dsl::testing::SceneTestAccess;

  class PaintPlatform : public NullScenePlatformController
  {
  public:
    struct Observation
    {
      ApplyPaintPlan plan;
      unsigned long ownerKey;
    };
    PaintPlatform()
        : count(0),
          queries(0),
          commits(0),
          applies(0),
          preLayoutCommits(0),
          refuseSurfaceProjection(false)
    {
    }
    virtual void beginApplyCycle()
    {
      count = queries = commits = applies = preLayoutCommits = 0;
    }
    virtual void onChange(Node *root, NodeDirtyFlags, bool)
    {
      LayoutState state;
      state.width = 640;
      state.height = 600;
      state.lineHeight = 20;
      this->projectLayoutForTesting(root, state);
    }
    virtual void
    onBoundaryApply(Node *root, BoundaryNode *owner, const BoundaryLocalApplyInfo &info, const PlatformApplyPlan &plan)
    {
      ++applies;
      const unsigned before = commits;
      NullScenePlatformController::onBoundaryApply(root, owner, info, plan);
      if (plan.hasLayoutWork() || plan.hasStructureWork())
        preLayoutCommits += commits - before;
    }
    virtual void onPaintPlanSubmitted(BoundaryNode *, const ApplyPaintPlan &plan)
    {
      LOKA_VERIFY(count < 32);
      observations[count].plan = plan;
      observations[count].ownerKey = this->paintScope().ownerKey;
      ++count;
    }
    virtual void onPaintQueried()
    {
      ++queries;
    }
    virtual void onPaintCommitted()
    {
      ++commits;
    }
    void forget(NullRectSurfaceContext &context)
    {
      context.invalidatePaintHistory();
    }
    /** Simulates a refused projection of every RectSurface while set. */
    virtual bool prepareProjectedLayout(Node *node, LayoutState &state)
    {
      if (refuseSurfaceProjection && node && node->asRectSurfaceNode())
        return false;
      return NullScenePlatformController::prepareProjectedLayout(node, state);
    }
    Observation observations[32];
    unsigned count, queries, commits, applies, preLayoutCommits;
    bool refuseSurfaceProjection;
  };
  void settle(Scene &scene)
  {
    for (int i = 0; scene.hasPendingInvalidation() && i < 12; ++i)
      LOKA_VERIFY(scene.flushInvalidation());
    assert(!scene.hasPendingInvalidation());
  }
  Node *find(Node *root, NodeKind kind, unsigned &index)
  {
    if (root->kind() == kind && index-- == 0)
      return root;
    INestable *nest = root->asNestable();
    for (Node *n = nest ? nest->childrenHead() : 0; n; n = n->nextInComposition)
    {
      Node *found = find(n, kind, index);
      if (found)
        return found;
    }
    return 0;
  }
  Node *find(Node *root, NodeKind kind)
  {
    unsigned index = 0;
    return find(root, kind, index);
  }
  RectSurfaceModel sprite(short x)
  {
    RectSurfaceModel value;
    value.rectCount = 1;
    value.rects[0] = RectSprite(x, 10, 8, 8);
    return value;
  }
  void change(Scene &scene, MutableState<RectSurfaceModel> &state, short x)
  {
    {
      StateTrackerGuard guard(SceneTestAccess::rootBoundary(scene)->tracker());
      state.set(sprite(x));
    }
    settle(scene);
  }
  /** A platform String that refuses UTF-8 materialization: nothing can render it. */
  class RefusingUtf8String : public loka::platform::String
  {
  public:
    virtual bool appendUtf8(std::string &) const
    {
      return false;
    }
  };
  String unrenderable()
  {
    return String(Managed<loka::platform::String>::Wrap(new RefusingUtf8String()));
  }
  void scoreValue(Scene &scene, MutableState<String> &state, const String &value)
  {
    {
      StateTrackerGuard guard(SceneTestAccess::rootBoundary(scene)->tracker());
      state.set(value);
    }
    settle(scene);
  }
  void score(Scene &scene, MutableState<String> &state, const char *value)
  {
    {
      StateTrackerGuard guard(SceneTestAccess::rootBoundary(scene)->tracker());
      state.set(String::Literal(value));
    }
    settle(scene);
  }
  const ApplyPaintPlan &only(PaintPlatform &platform)
  {
    LOKA_VERIFY(platform.count == 1);
    return platform.observations[0].plan;
  }
  void exactOne(PaintPlatform &platform)
  {
    LOKA_VERIFY(only(platform).precision() == APPLY_PAINT_EXACT);
    LOKA_VERIFY(only(platform).exactCount() == 1);
  }
  void refused(PaintPlatform &platform, PaintRefusalReason reason)
  {
    LOKA_VERIFY(only(platform).precision() == APPLY_PAINT_WIDENED);
    LOKA_VERIFY(only(platform).exactCount() == 0);
    LOKA_VERIFY(only(platform).widenReason() == APPLY_PAINT_WIDEN_REFUSED);
    LOKA_VERIFY(only(platform).refusalReason() == reason);
  }
  PaintQuery query(PaintPlatform &platform)
  {
    const PaintQuery q = {platform.paintScope(), PLACEMENT_ELIGIBLE};
    return q;
  }

  struct TreeData
  {
    TreeData()
        : a(String::Literal("AAAA")),
          b(String::Literal("BBBB")),
          selector(0),
          count(1),
          refusal(0),
          nested(false),
          shared(false),
          scroll(false)
    {
    }
    MutableState<RectSurfaceModel> models[9];
    MutableState<String> a, b;
    int selector, count, refusal;
    bool nested, shared, scroll;
  };
  class PaintTree;
  struct TreeProps : NodePropsBase<TreeProps>
  {
    typedef PaintTree NodeType;
    struct TypeTag
    {
    };
    TreeProps(TreeData *value = 0, int child = -1)
        : data(value),
          childIndex(child)
    {
    }
    bool operator<(const PropsBase &rhs) const
    {
      const TreeProps &other = static_cast<const TreeProps &>(rhs);
      return data != other.data ? data < other.data : childIndex < other.childIndex;
    }
    TreeData *data;
    int childIndex;
  };
  class PaintTree : public SceneTestSupport::
                        RecomposingBoundaryNode<PaintTree, TreeProps, true, StdCompositionBoundaryNodeBase<TreeProps> >
  {
  public:
    explicit PaintTree(const TreeProps &props)
        : SceneTestSupport::
              RecomposingBoundaryNode<PaintTree, TreeProps, true, StdCompositionBoundaryNodeBase<TreeProps> >(props)
    {
    }
    virtual void composeWithContext(ComponentContext &context, ComposeEvent event)
    {
      if (this->props.data->count == 0)
        SceneTestSupport::
            RecomposingBoundaryNode<PaintTree, TreeProps, true, StdCompositionBoundaryNodeBase<TreeProps> >::
                composeWithContext(context, event);
      else
        StdCompositionBoundaryNodeBase<TreeProps>::composeWithContext(context, event);
    }
    virtual bool flushViewDirtyImmediately(NodeDirtyFlags) const
    {
      return false;
    }
    virtual void composeNode(NodeComposition &c)
    {
      TreeData &d = *this->props.data;
      if (d.nested && this->props.childIndex < 0)
      {
        c.declare(VStack() << Boundary<PaintTree>(TreeProps(&d, 0)) << Boundary<PaintTree>(TreeProps(&d, 1))
                           << RectSurface(&d.models[8]).size(100, 60));
      }
      else
      {
        VStack stack;
        ImageView image;
        RectSurface surfaces[9];
        Text text = d.selector == 2 ? Text("CCCC") : Text(d.selector ? &d.b : &d.a);
        if (d.refusal == 1)
          stack << image;
        if (d.count == 0)
        {
          // selector 3: the same State drawn bold, a style-only change of a retained Text.
          if (d.selector == 3)
            stack << Text(&d.a).attr(TextAttr().weight(TEXT_WEIGHT_BOLD));
          else
            stack << text;
        }
        for (int i = 0; i < d.count; ++i)
        {
          const int index = this->props.childIndex < 0 ? i : d.shared ? 0 : this->props.childIndex;
          surfaces[i] = RectSurface(&d.models[index]).size(100, 60);
          stack << surfaces[i];
        }
        if (d.refusal == 2)
          stack << image;
        if (d.scroll)
          c.declare(Box().size(90, 40) << (ScrollView() << stack));
        else
          c.declare(stack);
      }
    }
  };
  void mount(Scene &scene, PaintPlatform &platform)
  {
    scene.mount(&platform);
    scene.updateAttached(true);
    settle(scene);
    platform.beginApplyCycle();
  }
} // namespace

void testRectSurfaceStateChangeExcludesSiblingTextFromPaintDamage()
{
  floppybird::SharedModel model;
  PaintPlatform platform;
  Scene scene(Boundary<floppybird::MainNode>(floppybird::MainProps(&model)));
  mount(scene, platform);
  change(scene, model.surfaceModel_, 10);
  change(scene, model.surfaceModel_, 12);
  exactOne(platform);
  const PaintDamage &d = only(platform).entry(0);
  LOKA_VERIFY(d.x == 10 && d.y == 30 && d.width == 10 && d.height == 8);
  LOKA_VERIFY(d.width < loka_floppy_bird::kWindowWidth && d.height < loka_floppy_bird::kWindowHeight);
  LOKA_VERIFY(d.y >= 20 && d.y + d.height <= 20 + loka_floppy_bird::kWindowHeight);
  LOKA_VERIFY(platform.queries == 2 && platform.commits == 2);
  scene.unmount();
}
void testTextStateChangeYieldsExactTextDamageOnly()
{
  floppybird::SharedModel model;
  PaintPlatform platform;
  Scene scene(Boundary<floppybird::MainNode>(floppybird::MainProps(&model)));
  mount(scene, platform);
  score(scene, model.scoreText_, "Score: 1");
  exactOne(platform);
  const PaintDamage &d = only(platform).entry(0);
  LOKA_VERIFY(d.x == 0 && d.y == 0 && d.width == 640 && d.height == 20);
  LOKA_VERIFY(d.coverage == PAINT_COVERAGE_ERASE_AND_PAINT);
  score(scene,
        model.scoreText_,
        "This unwrapped score is deliberately longer than the six hundred and forty pixel seat: one hundred and sixty "
        "characters are insufficient to fit all of this deliberately verbose score text.");
  refused(platform, PAINT_REFUSED_PLACEMENT_UNSETTLED);
  scene.unmount();
}
void testNativeControlAnswersNativeScheduledAndDoesNotWiden()
{
  smirkbench::SmirkModel model(640, 400, true);
  PaintPlatform platform;
  Scene scene(Boundary<smirkbench::MainNode>(smirkbench::MainProps(&model)));
  mount(scene, platform);
  model.advanceFrame(1.0 / 60.0);
  settle(scene);
  exactOne(platform);
  // The Button is a native control: the rail answers NATIVE_SCHEDULED by kind and
  // never queries (or casts) its context, so the plan above stayed exact with
  // the Button present and three residents were queried (Button, Text, surface).
  Node *button = find(SceneTestAccess::rootBoundary(scene), NODE_KIND_BUTTON);
  LOKA_VERIFY(button && button->getContext());
  LOKA_VERIFY(platform.queries == 3);
  scene.unmount();
}
void testRefusedDrawerWidensWholePlan()
{
  for (int order = 1; order <= 2; ++order)
  {
    TreeData data;
    data.refusal = order;
    PaintPlatform platform;
    Scene scene(Boundary<PaintTree>(TreeProps(&data)));
    mount(scene, platform);
    change(scene, data.models[0], 4);
    refused(platform, PAINT_REFUSED_NO_CONTEXT);
    LOKA_VERIFY(platform.queries == 2);
    scene.unmount();
  }
}
void testForeignContextIsNeverCastAndOwnedHandlersCannotBeReplaced()
{
  // A replaced ImageView handler installs a plain NodeContext (not a
  // NativeNodeContext). The paint walk must answer UNSUPPORTED_KIND for that
  // resident without casting it; a wrong cast is what the ASan run caught in the
  // SimpleViewer geometry fixtures before this rule existed.
  class PlainContext : public NodeContext
  {
  };
  class PlainImageHandler : public IPlatformNodeHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return NodeTypeToken<ImageViewNode>();
    }
    virtual NodeContext *ensureContext(Node *node, IPlatformController *, const LayoutState &)
    {
      if (!node->getContext())
        node->setContext(new PlainContext());
      return node->getContext();
    }
  } plainImage;
  TreeData data;
  data.refusal = 1;
  PaintPlatform platform;
  LOKA_VERIFY(platform.registerNodeHandler(&plainImage));
  Scene scene(Boundary<PaintTree>(TreeProps(&data)));
  mount(scene, platform);
  change(scene, data.models[0], 4);
  refused(platform, PAINT_REFUSED_UNSUPPORTED_KIND);
  scene.unmount();
  // The two owned-drawer handlers are addressed by concrete type in the
  // presenter, so a foreign handler for their kinds is refused at registration.
  class ForeignTextHandler : public IPlatformNodeHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return NullTextNodeHandlerKey();
    }
    virtual NodeContext *ensureContext(Node *, IPlatformController *, const LayoutState &)
    {
      return 0;
    }
  } foreignText;
  class ForeignSurfaceHandler : public IPlatformNodeHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return NullRectSurfaceNodeHandlerKey();
    }
    virtual NodeContext *ensureContext(Node *, IPlatformController *, const LayoutState &)
    {
      return 0;
    }
  } foreignSurface;
  PaintPlatform second;
  LOKA_VERIFY(!second.registerNodeHandler(&foreignText));
  LOKA_VERIFY(!second.registerNodeHandler(&foreignSurface));
}
void testCompressedParentIncludesNestedOwners()
{
  for (int refusal = 0; refusal < 2; ++refusal)
  {
    TreeData data;
    data.nested = true;
    data.refusal = refusal;
    PaintPlatform platform;
    Scene scene(Boundary<PaintTree>(TreeProps(&data)));
    mount(scene, platform);
    BoundaryNode *root = SceneTestAccess::rootBoundary(scene);
    root->markViewDirty(NODE_DIRTY_PROPS);
    {
      StateTrackerGuard guard(root->tracker());
      data.models[0].set(sprite(2));
      data.models[1].set(sprite(3));
    }
    settle(scene);
    LOKA_VERIFY(platform.applies == 1);
    if (refusal)
      refused(platform, PAINT_REFUSED_NO_CONTEXT);
    else
    {
      LOKA_VERIFY(only(platform).precision() == APPLY_PAINT_EXACT && only(platform).count() == 2);
      LOKA_VERIFY(only(platform).entry(0).y != only(platform).entry(1).y);
      LOKA_VERIFY(platform.queries == 3 && platform.commits == 3);
    }
    scene.unmount();
  }
}
void testSameStateTwoBoundariesDamagesBothOwners()
{
  TreeData data;
  data.nested = true;
  data.shared = true;
  PaintPlatform platform;
  Scene scene(Boundary<PaintTree>(TreeProps(&data)));
  mount(scene, platform);
  change(scene, data.models[0], 3);
  LOKA_VERIFY(platform.applies == 2 && platform.count == 2);
  LOKA_VERIFY(platform.observations[0].plan.precision() == APPLY_PAINT_EXACT);
  LOKA_VERIFY(platform.observations[1].plan.precision() == APPLY_PAINT_EXACT);
  LOKA_VERIFY(platform.observations[0].plan.entry(0).y != platform.observations[1].plan.entry(0).y);
  platform.beginApplyCycle();
  change(scene, data.models[0], 3);
  LOKA_VERIFY(platform.count == 0);
  Node *stack = SceneTestAccess::rootBoundary(scene)->childrenHead();
  Node *a = stack->asNestable()->childrenHead();
  a->asBoundary()->markViewDirty(NODE_DIRTY_PROPS);
  a->nextInComposition->asBoundary()->markViewDirty(NODE_DIRTY_PROPS);
  settle(scene);
  LOKA_VERIFY(platform.count == 2);
  LOKA_VERIFY(platform.observations[0].plan.precision() == APPLY_PAINT_NONE);
  LOKA_VERIFY(platform.observations[1].plan.precision() == APPLY_PAINT_NONE);
  scene.unmount();
}
void testUnknownHistoryRecoversThroughWidenedPresentation()
{
  TreeData data;
  PaintPlatform platform;
  Scene scene(Boundary<PaintTree>(TreeProps(&data)));
  mount(scene, platform);
  Node *surface = find(SceneTestAccess::rootBoundary(scene), NODE_KIND_RECT_SURFACE);
  platform.forget(*static_cast<NullRectSurfaceContext *>(surface->getContext()));
  change(scene, data.models[0], 3);
  refused(platform, PAINT_REFUSED_HISTORY_UNKNOWN);
  LOKA_VERIFY(platform.commits == 1);
  change(scene, data.models[0], 4);
  exactOne(platform);
  scene.unmount();
}
void testPlacementRefusals()
{
  TreeData data;
  PaintPlatform platform;
  Scene scene(Boundary<PaintTree>(TreeProps(&data)));
  mount(scene, platform);
  BoundaryNode *root = SceneTestAccess::rootBoundary(scene);
  root->markViewDirty(NODE_DIRTY_LAYOUT);
  settle(scene);
  LOKA_VERIFY(platform.count >= 2 && platform.preLayoutCommits == 0);
  LOKA_VERIFY(platform.observations[0].plan.refusalReason() == PAINT_REFUSED_PLACEMENT_UNSETTLED);
  Node *surface = find(root, NODE_KIND_RECT_SURFACE);
  NativeNodeContext *context = static_cast<NativeNodeContext *>(surface->getContext());
  LOKA_VERIFY(context->queryPaintDamage(query(platform)).kind == PAINT_ANSWER_EXACT);
  context->onFactChanged(NODE_FACT_ATTACHED, NODE_FACT_DETACHED_RETAINED);
  LOKA_VERIFY(context->queryPaintDamage(query(platform)).reason == PAINT_REFUSED_PLACEMENT_UNSETTLED);
  scene.unmount();
  NullScenePlatformController skipped;
  Scene second(Boundary<PaintTree>(TreeProps(&data)));
  skipped.skipNextProjectionForTesting();
  second.mount(&skipped);
  second.updateAttached(true);
  Node *unplaced = find(SceneTestAccess::rootBoundary(second), NODE_KIND_RECT_SURFACE);
  LOKA_VERIFY(unplaced && !unplaced->getContext());
  second.unmount();
  NullTextContext absent(0);
  LayoutState state;
  absent.layout(&skipped, state);
  const PaintQuery missing = {skipped.paintScope(), PLACEMENT_ELIGIBLE};
  LOKA_VERIFY(absent.queryPaintDamage(missing).reason == PAINT_REFUSED_PLACEMENT_UNSETTLED);
}
void testRetainedTextRebindsToNewStateAndCompares()
{
  TreeData data;
  data.count = 0;
  PaintPlatform platform;
  Scene scene(Boundary<PaintTree>(TreeProps(&data)));
  mount(scene, platform);
  BoundaryNode *root = SceneTestAccess::rootBoundary(scene);
  Node *text = find(root, NODE_KIND_TEXT);
  NodeContext *context = text->getContext();
  data.selector = 1;
  root->markViewDirty(NODE_DIRTY_PROPS);
  settle(scene);
  LOKA_VERIFY(find(root, NODE_KIND_TEXT) == text && text->getContext() == context);
  LOKA_VERIFY(text->asTextNode()->props.text_ == &data.b);
  exactOne(platform);
  platform.beginApplyCycle();
  score(scene, data.a, "DDDD");
  LOKA_VERIFY(platform.count == 0);
  score(scene, data.b, "EEEE");
  exactOne(platform);
  data.selector = 2;
  root->markViewDirty(NODE_DIRTY_PROPS);
  settle(scene);
  LOKA_VERIFY(find(root, NODE_KIND_TEXT) == text && text->getContext() == context);
  LOKA_VERIFY(text->asTextNode()->props.ownsText);
  exactOne(platform);
  platform.beginApplyCycle();
  score(scene, data.b, "FFFF");
  LOKA_VERIFY(platform.count == 0);
  scene.unmount();
}
void testCapacityOverflowWidens()
{
  for (int count = 8; count <= 9; ++count)
  {
    TreeData data;
    data.count = 9;
    PaintPlatform platform;
    Scene scene(Boundary<PaintTree>(TreeProps(&data)));
    mount(scene, platform);
    {
      StateTrackerGuard guard(SceneTestAccess::rootBoundary(scene)->tracker());
      for (int i = 0; i < count; ++i)
        data.models[i].set(sprite(3));
    }
    settle(scene);
    if (count == 8)
      LOKA_VERIFY(only(platform).precision() == APPLY_PAINT_EXACT && only(platform).exactCount() == 8);
    else
    {
      LOKA_VERIFY(only(platform).precision() == APPLY_PAINT_WIDENED && only(platform).exactCount() == 0);
      LOKA_VERIFY(only(platform).widenReason() == APPLY_PAINT_WIDEN_CAPACITY);
    }
    LOKA_VERIFY(platform.queries == 9 && platform.commits == 9);
    scene.unmount();
  }
}
void testPlanHoldsNoPointersAndIsVisitLocal()
{
  ApplyPaintPlan observation;
  {
    TreeData data;
    PaintPlatform platform;
    Scene scene(Boundary<PaintTree>(TreeProps(&data)));
    mount(scene, platform);
    change(scene, data.models[0], 3);
    observation = only(platform);
    scene.unmount();
  }
  {
    TreeData data;
    PaintPlatform platform;
    Scene scene(Boundary<PaintTree>(TreeProps(&data)));
    mount(scene, platform);
    change(scene, data.models[0], 9);
    LOKA_VERIFY(only(platform).entry(0).x == 9);
    scene.unmount();
  }
  // Source-reviewed types contain enums, integer coordinates and fixed value arrays only.
  LOKA_VERIFY(observation.precision() == APPLY_PAINT_EXACT && observation.entry(0).width == 8);
  LOKA_VERIFY(observation.entry(0).scope.ownerKey == 1);
}
void testPaintLifecycleAndScope()
{
  TreeData data;
  data.scroll = true;
  PaintPlatform platform;
  Scene scene(Boundary<PaintTree>(TreeProps(&data)));
  mount(scene, platform);
  change(scene, data.models[0], 3);
  refused(platform, PAINT_REFUSED_PLACEMENT_UNSETTLED);
  LOKA_VERIFY(platform.commits == 0);
  scene.unmount();
  class Census : public IPaintResidentVisitor
  {
  public:
    Census()
        : count(0)
    {
    }
    unsigned count;
    virtual void visit(Node *, NodeContext *, BoundaryNode *)
    {
      ++count;
    }
  } census;
  PaintTree unattached((TreeProps(&data)));
  NotifySubtreeNodeDetached(&unattached);
  enumerateAttachedResidents(&unattached, census);
  LOKA_VERIFY(census.count == 0);
  class ParkedRoot : public PaintTree
  {
  public:
    explicit ParkedRoot(const TreeProps &props)
        : PaintTree(props)
    {
    }
    virtual Node *retainedLifecycleBranch(unsigned index)
    {
      return index == 0 ? &this->parked_ : 0;
    }

  private:
    Node parked_;
  } parked((TreeProps(&data)));
  enumerateAttachedResidents(&parked, census);
  LOKA_VERIFY(census.count == 1); // Only the root, even before a parked fact is delivered.

  class UnsupportedRoot : public PaintTree, public IProjectedLayoutNode
  {
  public:
    explicit UnsupportedRoot(const TreeProps &props)
        : PaintTree(props)
    {
    }
    virtual IProjectedLayoutNode *asProjectedLayoutNode()
    {
      return this;
    }
    virtual short layoutProjected(IPlatformController *, LayoutState &state)
    {
      return state.y;
    }
  } unsupported((TreeProps(&data)));
  platform.beginApplyCycle();
  BoundaryLocalApplyInfo info;
  info.paintKind = LOCAL_APPLY_PAINT_GENERIC;
  PlatformApplyPlan plan;
  plan.paintKind = PlatformApplyPlan::PAINT_LOCAL;
  plan.setPrimaryRoot(&unsupported);
  platform.onBoundaryApply(&unsupported, &unsupported, info, plan);
  refused(platform, PAINT_REFUSED_UNSUPPORTED_KIND);
}

void testPaintPolicyScopeAndLifecycleInvalidation()
{
  floppybird::SharedModel model;
  PaintPlatform platform;
  Scene scene(Boundary<floppybird::MainNode>(floppybird::MainProps(&model)));
  mount(scene, platform);
  BoundaryNode *root = SceneTestAccess::rootBoundary(scene);
  RectSurfaceNode *surface = find(root, NODE_KIND_RECT_SURFACE)->asRectSurfaceNode();
  TextNode *text = find(root, NODE_KIND_TEXT)->asTextNode();
  NativeNodeContext *surfaceContext = static_cast<NativeNodeContext *>(surface->getContext());
  NativeNodeContext *textContext = static_cast<NativeNodeContext *>(text->getContext());
  change(scene, model.surfaceModel_, 4);
  PaintQuery q = query(platform);
  ++q.scope.ownerKey;
  LOKA_VERIFY(surfaceContext->queryPaintDamage(q).reason == PAINT_REFUSED_PLACEMENT_UNSETTLED);
  LOKA_VERIFY(textContext->queryPaintDamage(q).reason == PAINT_REFUSED_PLACEMENT_UNSETTLED);
  // Query-time pixel policy is intentionally exercised without a layout pass.
  surface->props.clearBackground_ = false;
  const PaintAnswer policy = surfaceContext->queryPaintDamage(query(platform));
  LOKA_VERIFY(policy.kind == PAINT_ANSWER_EXACT && policy.damage.coverage == PAINT_COVERAGE_ERASE_AND_PAINT);
  LOKA_VERIFY(policy.damage.width == loka_floppy_bird::kWindowWidth
              && policy.damage.height == loka_floppy_bird::kWindowHeight);
  surface->props.clearBackground_ = true;
  RectSurfaceModel hints = model.surfaceModel_.get();
  hints.dirtyRectCount = 1;
  hints.dirtyRects[0] = RectSurfaceModel::DirtyRect(0, 0, 100, 100);
  {
    StateTrackerGuard guard(root->tracker());
    model.surfaceModel_.set(hints);
  }
  settle(scene);
  LOKA_VERIFY(only(platform).precision() == APPLY_PAINT_NONE);
  text->props.hasAttr_ = true;
  text->props.attr_.weight(TEXT_WEIGHT_BOLD);
  LOKA_VERIFY(textContext->queryPaintDamage(query(platform)).reason == PAINT_REFUSED_PLACEMENT_UNSETTLED);
  text->props.hasAttr_ = false;
  NotifySubtreeNodeDetached(root);
  LifecycleFactTestAccess::DeliverFacts(root);
  LOKA_VERIFY(surfaceContext->queryPaintDamage(query(platform)).reason == PAINT_REFUSED_PLACEMENT_UNSETTLED);
  LOKA_VERIFY(textContext->queryPaintDamage(query(platform)).reason == PAINT_REFUSED_PLACEMENT_UNSETTLED);
  class Census : public IPaintResidentVisitor
  {
  public:
    Census()
        : count(0)
    {
    }
    unsigned count;
    virtual void visit(Node *, NodeContext *, BoundaryNode *)
    {
      ++count;
    }
  } census;
  enumerateAttachedResidents(root, census);
  LOKA_VERIFY(census.count == 0);
  NotifySubtreeNodeAttached(root);
  LifecycleFactTestAccess::DeliverFacts(root);
  LOKA_VERIFY(surfaceContext->queryPaintDamage(query(platform)).reason == PAINT_REFUSED_PLACEMENT_UNSETTLED);
  LayoutState layout;
  layout.width = 640;
  layout.height = 600;
  layout.lineHeight = 20;
  platform.projectLayoutForTesting(root, layout);
  LOKA_VERIFY(surfaceContext->queryPaintDamage(query(platform)).kind == PAINT_ANSWER_EXACT);
  LOKA_VERIFY(textContext->queryPaintDamage(query(platform)).kind == PAINT_ANSWER_EXACT);
  LifecycleFactTestAccess::MarkSubtreeRetired(root);
  LifecycleFactTestAccess::DeliverFacts(root);
  LOKA_VERIFY(surfaceContext->queryPaintDamage(query(platform)).reason == PAINT_REFUSED_PLACEMENT_UNSETTLED);
  LOKA_VERIFY(textContext->queryPaintDamage(query(platform)).reason == PAINT_REFUSED_PLACEMENT_UNSETTLED);
  scene.unmount();
}
void testPaintPlanEmptyCapacityAndScopeValues()
{
  const PaintScope scope = {1, 0, 0, 0, 0, 100, 100};
  PaintScope other = scope;
  LOKA_VERIFY(other == scope);
  ++other.originX;
  LOKA_VERIFY(other != scope);
  other = scope;
  ++other.originY;
  LOKA_VERIFY(other != scope);
  other = scope;
  ++other.clipX;
  LOKA_VERIFY(other != scope);
  other = scope;
  ++other.clipY;
  LOKA_VERIFY(other != scope);
  other = scope;
  ++other.clipWidth;
  LOKA_VERIFY(other != scope);
  other = scope;
  ++other.clipHeight;
  LOKA_VERIFY(other != scope);
  other = scope;
  ++other.ownerKey;
  LOKA_VERIFY(other != scope);
  ApplyPaintPlan plan;
  const PaintDamage damage = {scope, 1, 2, 3, 4, PAINT_COVERAGE_ERASE_AND_PAINT};
  for (int i = 0; i < 8; ++i)
    LOKA_VERIFY(plan.addExact(damage));
  PaintDamage empty = damage;
  empty.width = 0;
  LOKA_VERIFY(plan.addExact(empty) && plan.count() == 8);
  LOKA_VERIFY(!plan.addExact(damage));
  plan.widen(APPLY_PAINT_WIDEN_CAPACITY, scope);
  LOKA_VERIFY(!plan.addExact(damage));
  plan.widen(APPLY_PAINT_WIDEN_REFUSED, other, PAINT_REFUSED_NO_CONTEXT);
  LOKA_VERIFY(plan.exactCount() == 0 && plan.count() == 1);
  LOKA_VERIFY(plan.entry(0).scope == scope && plan.widenReason() == APPLY_PAINT_WIDEN_CAPACITY);
}

void testUnrenderableTextNeverBecomesPresented()
{
  // A String whose platform refuses materialization measures as nothing. That
  // measurement must not pass the seat check, and the value must never be
  // committed as presented; otherwise a later identical handle compares equal
  // and answers EXACT with empty damage for pixels that were never drawn.
  floppybird::SharedModel model;
  PaintPlatform platform;
  Scene scene(Boundary<floppybird::MainNode>(floppybird::MainProps(&model)));
  mount(scene, platform);
  scoreValue(scene, model.scoreText_, unrenderable());
  refused(platform, PAINT_REFUSED_PLACEMENT_UNSETTLED);
  // The widened reconstruction must not have committed the unrenderable value.
  scoreValue(scene, model.scoreText_, unrenderable());
  refused(platform, PAINT_REFUSED_HISTORY_UNKNOWN);
  // A renderable value recovers through the widened presentation, then compares exactly.
  score(scene, model.scoreText_, "Score: 2");
  refused(platform, PAINT_REFUSED_HISTORY_UNKNOWN);
  score(scene, model.scoreText_, "Score: 3");
  exactOne(platform);
  scene.unmount();
}

void testRefusedReprojectionInvalidatesPlacement()
{
  // A placed surface whose re-projection is refused before its layout() runs
  // must lose its placement and history in that pass: the post-projection
  // completion walk may commit only residents the pass actually placed.
  floppybird::SharedModel model;
  PaintPlatform platform;
  Scene scene(Boundary<floppybird::MainNode>(floppybird::MainProps(&model)));
  mount(scene, platform);
  change(scene, model.surfaceModel_, 4);
  exactOne(platform);
  platform.refuseSurfaceProjection = true;
  scene.requestInvalidate(NODE_DIRTY_LAYOUT);
  settle(scene);
  platform.beginApplyCycle();
  change(scene, model.surfaceModel_, 6);
  refused(platform, PAINT_REFUSED_PLACEMENT_UNSETTLED);
  LOKA_VERIFY(platform.commits == 1); // the Text was placed and reconstructed; the surface was not
  platform.refuseSurfaceProjection = false;
  scene.requestInvalidate(NODE_DIRTY_LAYOUT);
  settle(scene);
  platform.beginApplyCycle();
  change(scene, model.surfaceModel_, 8);
  exactOne(platform);
  scene.unmount();
}

void testStyleOnlyApplyRecoversTextHistory()
{
  // A retained Text whose resolved style changes through a props-only apply has
  // no layout pass to refresh its placed style. The query refuses (widen), and
  // the widened presentation must adopt the current style and re-establish the
  // history, instead of refusing forever until an unrelated layout.
  TreeData data;
  data.count = 0;
  PaintPlatform platform;
  Scene scene(Boundary<PaintTree>(TreeProps(&data)));
  mount(scene, platform);
  BoundaryNode *root = SceneTestAccess::rootBoundary(scene);
  Node *text = find(root, NODE_KIND_TEXT);
  data.selector = 3;
  root->markViewDirty(NODE_DIRTY_PROPS);
  settle(scene);
  LOKA_VERIFY(find(root, NODE_KIND_TEXT) == text);
  refused(platform, PAINT_REFUSED_PLACEMENT_UNSETTLED);
  platform.beginApplyCycle();
  score(scene, data.a, "GGGG");
  exactOne(platform);
  scene.unmount();
}
