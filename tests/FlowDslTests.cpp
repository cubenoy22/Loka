#include "FlowDslTests.hpp"

#include <cassert>
#include <cstring>
#include <cstdio>
#include <vector>

#include "app/nodes/nestable/Box.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/controls/Cell.hpp"
#include "app/nodes/nestable/Fragment.hpp"
#include "app/nodes/nestable/Fragment.hpp"
#include "app/nodes/nestable/Grid.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/nestable/Show.hpp"
#include "app/nodes/Text.hpp"
#include "app/nodes/nestable/ZStack.hpp"
#include "app/scene/composition/NodeComposition.hpp"
#include "app/scene/projection/PlatformController.hpp"
#include "app/scene/Scene.hpp"
#include "app/scene/state/FlowSlot.hpp"
#include "support/Headless.hpp"
#include "dsl/flow/Expr.hpp"
#include "dsl/stream/StateStream.hpp"
#include "../example/HelloWorld/src/MainNode.hpp"
#include "../example/MineSweeper/src/MainNode.hpp"
#include "../example/SimpleViewer/src/MainNode.hpp"
#include "../example/SimpleViewer/src/SimpleViewerFlowAdapters.hpp"
#include "core/State.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "testing/scene/SceneTestFlow.hpp"

namespace
{
  static loka::app::HStack buildTypedHStack()
  {
    return loka::app::HStack().alignVertical(loka::app::VERTICAL_ALIGNMENT_BOTTOM)
           << loka::app::Text("Left") << loka::app::Text("Right");
  }

  static loka::app::VStack buildTypedVStack()
  {
    return loka::app::VStack().alignHorizontal(loka::app::HORIZONTAL_ALIGNMENT_CENTER)
           << loka::app::Text("Top") << loka::app::Text("Bottom");
  }

  static loka::app::Box buildTypedBox()
  {
    return loka::app::Box().padding(8).testId("TypedBox") << loka::app::Text("Box child");
  }

  static loka::app::Fragment buildTypedFragment()
  {
    return loka::app::Fragment() << loka::app::Text("Fragment A") << loka::app::Text("Fragment B");
  }

  static loka::app::Grid buildTypedGrid()
  {
    return loka::app::Grid().rows(1).cols(2) << loka::app::Text("Grid A") << loka::app::Text("Grid B");
  }

  static loka::app::ZStack buildTypedZStack()
  {
    return loka::app::ZStack().testId("TypedZStack") << loka::app::Text("Back") << loka::app::Text("Front");
  }

  static loka::app::Fragment buildTypedInlineFragment()
  {
    return loka::app::Fragment() << loka::app::Text("Light child").testId("TypedDslLightChild");
  }

  class TypedDslLightHostNode;
  typedef loka::app::scene::BoundaryPropsFor<TypedDslLightHostNode> TypedDslLightHostProps;

  class TypedDslLightHostNode : public loka::app::scene::BoundaryNodeFor<TypedDslLightHostNode>
  {
  public:
    TypedDslLightHostNode(const TypedDslLightHostProps &p)
        : loka::app::scene::BoundaryNodeFor<TypedDslLightHostNode>(TypedDslLightHostProps(p))
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      c.declare(loka::app::HStack().testId("TypedDslLightRow") << buildTypedInlineFragment());
    }
  };

  struct FlowTestMarkerContext
  {
    std::vector<int> *order;
    int marker;
  };

  struct FlowErrorCapture
  {
    int kind;
    int code;
    int calls;
  };

  class ConditionalProjectedProbeNode;
  struct ConditionalProjectedProbeTypeTag
  {
  };

  struct ConditionalProjectedProbeProps : public loka::app::scene::NodePropsBase<ConditionalProjectedProbeProps>
  {
    typedef ConditionalProjectedProbeTypeTag TypeTag;
    typedef ConditionalProjectedProbeNode NodeType;
    ConditionalProjectedProbeProps() {}
    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      return rhs.propsTypeId() == this->propsTypeId() ? false : this->propsTypeId() < rhs.propsTypeId();
    }
  };

  static int g_conditionalProjectedProbeLayoutCalls = 0;
  static int g_conditionalProjectedProbePrepareCalls = 0;

  class ConditionalProjectedProbeNode : public loka::app::scene::Node, public loka::app::scene::IProjectedLayoutNode
  {
  public:
    typedef ConditionalProjectedProbeTypeTag TypeTag;
    ConditionalProjectedProbeProps props;
    ConditionalProjectedProbeNode(const ConditionalProjectedProbeProps &p)
        : props(p)
    {
    }
    virtual loka::app::scene::IProjectedLayoutNode *asProjectedLayoutNode()
    {
      return this;
    }
    virtual short layoutProjected(loka::app::scene::IPlatformController *controller,
                                  loka::app::scene::LayoutState &state)
    {
      ++g_conditionalProjectedProbeLayoutCalls;
      const bool prepared = loka::app::scene::PrepareProjectedLayout(controller, this, state);
      return static_cast<short>(prepared ? state.y + 1 : state.y);
    }
  };

  struct ConditionalProjectedProbeDefinition
      : public loka::app::scene::NodeDefinition<ConditionalProjectedProbeProps, ConditionalProjectedProbeNode>
  {
    ConditionalProjectedProbeDefinition()
        : loka::app::scene::NodeDefinition<ConditionalProjectedProbeProps, ConditionalProjectedProbeNode>()
    {
    }
    ConditionalProjectedProbeDefinition(const ConditionalProjectedProbeProps &p)
        : loka::app::scene::NodeDefinition<ConditionalProjectedProbeProps, ConditionalProjectedProbeNode>(p)
    {
    }
  };

  class ConditionalProjectedProbeController : public loka::app::scene::IPlatformController
  {
  public:
    ConditionalProjectedProbeController()
        : lastRoot_(0),
          lastFlags_(loka::app::scene::NODE_DIRTY_NONE),
          lastFullRebuild_(false),
          changeCalls_(0)
    {
    }

    virtual void onChange(loka::app::scene::Node *rootNode, loka::app::scene::NodeDirtyFlags flags, bool fullRebuild)
    {
      lastRoot_ = rootNode;
      lastFlags_ = flags;
      lastFullRebuild_ = fullRebuild;
      ++changeCalls_;
      if (!rootNode)
      {
        return;
      }
      loka::app::scene::LayoutState state;
      state.x = 0;
      state.y = 0;
      state.width = 100;
      state.height = 100;
      state.lineHeight = 0;
      state.spacing = 0;
      walkTree(this, rootNode, state);
    }

    virtual bool prepareProjectedLayout(loka::app::scene::Node *node, loka::app::scene::LayoutState &state)
    {
      (void)state;
      ++g_conditionalProjectedProbePrepareCalls;
      if (node && !node->getContext())
      {
        node->setContext(new loka::app::scene::NodeContext(node));
      }
      return true;
    }

    virtual void synchronize() {}
    virtual bool hasPendingSync() const
    {
      return false;
    }
    virtual void destroy() {}

    static void walkTree(loka::app::scene::IPlatformController *controller,
                         loka::app::scene::Node *node,
                         const loka::app::scene::LayoutState &state)
    {
      if (!node)
      {
        return;
      }
      if (loka::app::scene::IProjectedLayoutNode *projected = node->asProjectedLayoutNode())
      {
        loka::app::scene::LayoutState projectedState = state;
        projected->layoutProjected(controller, projectedState);
      }
      if (loka::app::scene::INestable *nestable = node->asNestable())
      {
        loka::dsl::CompositionCursor<loka::app::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
        for (loka::app::scene::Node *child = it.next(); child; child = it.next())
        {
          walkTree(controller, child, state);
        }
      }
    }

    loka::app::scene::Node *lastRoot_;
    loka::app::scene::NodeDirtyFlags lastFlags_;
    bool lastFullRebuild_;
    int changeCalls_;
  };

  class PendingLayoutBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<PendingLayoutBoundaryNode> PendingLayoutBoundaryProps;
  static loka::core::MutableState<int> g_pendingLayoutWidthState(32);
  struct RecordingDirtySourceRegistrar : public loka::app::scene::DirtySourceRegistrar
  {
    RecordingDirtySourceRegistrar()
        : calls(0),
          lastState(0),
          lastFlags(loka::app::scene::NODE_DIRTY_NONE)
    {
    }

    virtual void markDirtyOnChange(loka::core::StateBase *state, loka::app::scene::NodeDirtyFlags flags)
    {
      ++calls;
      lastState = state;
      lastFlags = flags;
    }

    int calls;
    loka::core::StateBase *lastState;
    loka::app::scene::NodeDirtyFlags lastFlags;
  };
  class BoundaryLookupTestApi
  {
  public:
    static const char *kInterfaceName()
    {
      return "BoundaryLookupTestApi";
    }
    static BoundaryLookupTestApi *fromNode(loka::app::scene::Node *node)
    {
      if (!node)
      {
        return 0;
      }
      return static_cast<BoundaryLookupTestApi *>(node->queryInterface(kInterfaceName()));
    }
    virtual ~BoundaryLookupTestApi() {}
    virtual int id() const = 0;
  };

  class BoundaryLookupTestNode : public loka::app::scene::Node, public BoundaryLookupTestApi
  {
  public:
    explicit BoundaryLookupTestNode(int value)
        : value_(value)
    {
    }

    virtual void *queryInterface(const char *name)
    {
      return name && std::strcmp(name, kInterfaceName()) == 0 ? static_cast<BoundaryLookupTestApi *>(this) : 0;
    }

    virtual int id() const
    {
      return value_;
    }

  private:
    int value_;
  };

  class BoundaryLookupStateApi
  {
  public:
    static const char *kInterfaceName()
    {
      return "BoundaryLookupStateApi";
    }
    static BoundaryLookupStateApi *fromNode(loka::app::scene::Node *node)
    {
      if (!node)
      {
        return 0;
      }
      return static_cast<BoundaryLookupStateApi *>(node->queryInterface(kInterfaceName()));
    }
    virtual ~BoundaryLookupStateApi() {}
    virtual loka::app::scene::BorrowedState<int> countState() const = 0;
  };

  class BoundaryLookupStateNode : public loka::app::scene::Node, public BoundaryLookupStateApi
  {
  public:
    explicit BoundaryLookupStateNode(int value)
        : count_(value)
    {
    }

    virtual void *queryInterface(const char *name)
    {
      return name && std::strcmp(name, kInterfaceName()) == 0 ? static_cast<BoundaryLookupStateApi *>(this) : 0;
    }

    virtual loka::app::scene::BorrowedState<int> countState() const
    {
      return loka::app::scene::BorrowedState<int>(const_cast<loka::core::MutableState<int> *>(&count_));
    }

  private:
    loka::core::MutableState<int> count_;
  };

  // Owning test double: adopted states belong to the owner, releaseState
  // deletes, and anything still adopted is freed on teardown so ASan runs
  // stay leak-clean. Unlike the real BoundaryNode owner it does not register
  // states with a StateTracker and never deals with arena-allocated states,
  // so do not reuse it where tracker integration matters.
  class DummyStateOwner : public loka::app::scene::IStateOwner
  {
  public:
    DummyStateOwner()
        : tracker_(0)
    {
    }
    virtual ~DummyStateOwner()
    {
      for (size_t i = owned_.size(); i > 0; --i)
      {
        delete owned_[i - 1];
      }
    }

    virtual void adoptState(loka::core::StateBase *state)
    {
      adoptStateUnchecked(state);
    }
    virtual void adoptStateUnchecked(loka::core::StateBase *state)
    {
      if (!state)
      {
        return;
      }
      for (size_t i = 0; i < owned_.size(); ++i)
      {
        if (owned_[i] == state)
        {
          return;
        }
      }
      owned_.push_back(state);
    }
    virtual void releaseState(loka::core::StateBase *state)
    {
      if (!state)
      {
        return;
      }
      for (size_t i = 0; i < owned_.size();)
      {
        if (owned_[i] == state)
        {
          owned_.erase(owned_.begin() + i);
        }
        else
        {
          ++i;
        }
      }
      delete state;
    }
    virtual void reserveStates(size_t) {}
    virtual void reserveStateArena(size_t) {}
    virtual void *allocateStateMemory(size_t, size_t)
    {
      return 0;
    }
    virtual void registerStateMemory(loka::core::StateBase *, void (*)(loka::core::StateBase *)) {}
    virtual loka::core::StateTracker *tracker()
    {
      return tracker_;
    }

  private:
    loka::core::StateTracker *tracker_;
    std::vector<loka::core::StateBase *> owned_;
  };

  class PendingApplyProbeBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<PendingApplyProbeBoundaryNode> PendingApplyProbeBoundaryProps;
  static int g_pendingApplyCallCount = 0;
  static int g_pendingApplyWhileApplyingCount = 0;
  static loka::app::scene::BoundaryNode *g_pendingApplyLastLayoutRoot = 0;
  static loka::app::scene::BoundaryNode *g_pendingApplyLastPaintRoot = 0;
  static int g_defaultApplyLocalPaintCalls = 0;
  static int g_defaultApplyOpaquePaintCalls = 0;
  static int g_defaultApplyCompositedPaintCalls = 0;
  static int g_defaultApplyLayoutCalls = 0;
  static int g_defaultApplyStructureCalls = 0;
  static int g_defaultApplyBoundsWidth = 0;
  static int g_defaultApplyBoundsHeight = 0;
  static int g_defaultApplyOpaqueHintSeen = 0;
  class PendingCompositedProbeBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<PendingCompositedProbeBoundaryNode> PendingCompositedProbeBoundaryProps;
  class PendingApplySiblingABoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<PendingApplySiblingABoundaryNode> PendingApplySiblingABoundaryProps;
  class PendingApplySiblingBBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<PendingApplySiblingBBoundaryNode> PendingApplySiblingBBoundaryProps;
  class PendingApplySiblingsRootNode;
  typedef loka::app::scene::BoundaryPropsFor<PendingApplySiblingsRootNode> PendingApplySiblingsRootProps;
  static int g_pendingApplySiblingACalls = 0;
  static int g_pendingApplySiblingBCalls = 0;
  static loka::app::scene::BoundaryNode *g_pendingApplySiblingALayoutRoot = 0;
  static loka::app::scene::BoundaryNode *g_pendingApplySiblingAPaintRoot = 0;
  static loka::app::scene::BoundaryNode *g_pendingApplySiblingBLayoutRoot = 0;
  static loka::app::scene::BoundaryNode *g_pendingApplySiblingBPaintRoot = 0;
  class PendingApplyDefersSiblingABoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<PendingApplyDefersSiblingABoundaryNode>
      PendingApplyDefersSiblingABoundaryProps;
  class PendingApplyDefersSiblingBBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<PendingApplyDefersSiblingBBoundaryNode>
      PendingApplyDefersSiblingBBoundaryProps;
  class PendingApplyDefersSiblingsRootNode;
  typedef loka::app::scene::BoundaryPropsFor<PendingApplyDefersSiblingsRootNode> PendingApplyDefersSiblingsRootProps;
  static int g_pendingApplyDefersSiblingACalls = 0;
  static int g_pendingApplyDefersSiblingBCalls = 0;
  static bool g_pendingApplyDefersSiblingAQueuedSiblingB = false;
  static loka::app::scene::BoundaryNode *g_pendingApplyDeferredSiblingB = 0;
  class HeadlessScopeProbeBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<HeadlessScopeProbeBoundaryNode> HeadlessScopeProbeProps;
  class HeadlessScopeHostBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<HeadlessScopeHostBoundaryNode> HeadlessScopeHostProps;
  static HeadlessScopeProbeBoundaryNode *g_headlessScopeProbe = 0;
  static HeadlessScopeHostBoundaryNode *g_headlessScopeHost = 0;
  static int g_headlessScopeAttachCount = 0;
  static int g_headlessScopeDetachCount = 0;
  class HeadlessOwnedProbeNode;
  typedef loka::app::scene::HeadlessPropsFor<HeadlessOwnedProbeNode> HeadlessOwnedProbeProps;
  class HeadlessOwnedHostBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<HeadlessOwnedHostBoundaryNode> HeadlessOwnedHostProps;
  static HeadlessOwnedProbeNode *g_headlessOwnedProbe = 0;
  static HeadlessOwnedHostBoundaryNode *g_headlessOwnedHost = 0;
  static int g_headlessOwnedAttachCount = 0;
  static int g_headlessOwnedDestroyCount = 0;
  class HeadlessOwnedMultiProbeANode;
  typedef loka::app::scene::HeadlessPropsFor<HeadlessOwnedMultiProbeANode> HeadlessOwnedMultiProbeAProps;
  class HeadlessOwnedMultiProbeBNode;
  typedef loka::app::scene::HeadlessPropsFor<HeadlessOwnedMultiProbeBNode> HeadlessOwnedMultiProbeBProps;
  class HeadlessOwnedMultiHostBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<HeadlessOwnedMultiHostBoundaryNode> HeadlessOwnedMultiHostProps;
  static HeadlessOwnedMultiProbeANode *g_headlessOwnedMultiProbeA = 0;
  static HeadlessOwnedMultiProbeBNode *g_headlessOwnedMultiProbeB = 0;
  static HeadlessOwnedMultiHostBoundaryNode *g_headlessOwnedMultiHost = 0;
  static int g_headlessOwnedMultiAttachCountA = 0;
  static int g_headlessOwnedMultiAttachCountB = 0;
  static int g_headlessOwnedMultiDestroyCountA = 0;
  static int g_headlessOwnedMultiDestroyCountB = 0;
  class HeadlessOwnedPersistentProbeNode;
  typedef loka::app::scene::HeadlessPropsFor<HeadlessOwnedPersistentProbeNode> HeadlessOwnedPersistentProbeProps;
  class HeadlessOwnedMixedHostBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<HeadlessOwnedMixedHostBoundaryNode> HeadlessOwnedMixedHostProps;
  static HeadlessOwnedPersistentProbeNode *g_headlessOwnedPersistentProbe = 0;
  static int g_headlessOwnedPersistentAttachCount = 0;
  static int g_headlessOwnedPersistentDestroyCount = 0;

  class PendingLayoutBoundaryNode : public loka::app::scene::BoundaryNodeFor<PendingLayoutBoundaryNode>
  {
  public:
    PendingLayoutBoundaryNode(const PendingLayoutBoundaryProps &p)
        : loka::app::scene::BoundaryNodeFor<PendingLayoutBoundaryNode>(PendingLayoutBoundaryProps(p))
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      this->setLayoutBounds(0, 0, g_pendingLayoutWidthState.get(), 12);
      c.declare(loka::app::Text("Sized").testId("PendingLayoutText"));
    }

    virtual void declareDirtySources(loka::app::scene::DirtySourceRegistrar &registrar)
    {
      registrar.markDirtyOnChange(&g_pendingLayoutWidthState, loka::app::scene::NODE_DIRTY_PROPS);
    }
  };

  class PendingApplyProbeBoundaryNode : public loka::app::scene::BoundaryNodeFor<PendingApplyProbeBoundaryNode>
  {
  public:
    PendingApplyProbeBoundaryNode(const PendingApplyProbeBoundaryProps &p)
        : loka::app::scene::BoundaryNodeFor<PendingApplyProbeBoundaryNode>(PendingApplyProbeBoundaryProps(p))
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      c.declare(loka::app::Text("Apply").testId("PendingApplyText"));
    }

    virtual void applyPendingUpdate(const loka::app::scene::PlatformApplyPlan &plan)
    {
      const LocalApplyInfo info = this->localApplyInfo(plan);
      ++g_pendingApplyCallCount;
      assert(info.hasAnyWork());
      assert(info.hasRootedWork());
      assert(plan.hasAnyLocalWork(this));
      if (this->isApplyingPlatform())
      {
        ++g_pendingApplyWhileApplyingCount;
      }
      g_pendingApplyLastLayoutRoot = plan.layoutRoot;
      g_pendingApplyLastPaintRoot = plan.paintRoot;
      assert(info.hasPaintWork());
      assert(info.hasGenericPaintWork() || info.hasOpaquePaintWork());
      assert(plan.paintKind == loka::app::scene::PlatformApplyPlan::PAINT_LOCAL
             || plan.paintKind == loka::app::scene::PlatformApplyPlan::PAINT_LOCAL_OPAQUE);
    }
  };

  class PendingCompositedProbeBoundaryNode
      : public loka::app::scene::BoundaryNodeFor<PendingCompositedProbeBoundaryNode>
  {
  public:
    PendingCompositedProbeBoundaryNode(const PendingCompositedProbeBoundaryProps &p)
        : loka::app::scene::BoundaryNodeFor<PendingCompositedProbeBoundaryNode>(PendingCompositedProbeBoundaryProps(p))
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      this->noteCompositedPaint();
      c.declare(loka::app::Text("Composited").testId("PendingCompositedText"));
    }
  };

  class PendingDefaultApplyProbeBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<PendingDefaultApplyProbeBoundaryNode>
      PendingDefaultApplyProbeBoundaryProps;

  class PendingDefaultApplyProbeBoundaryNode
      : public loka::app::scene::BoundaryNodeFor<PendingDefaultApplyProbeBoundaryNode>
  {
  public:
    PendingDefaultApplyProbeBoundaryNode(const PendingDefaultApplyProbeBoundaryProps &p)
        : loka::app::scene::BoundaryNodeFor<PendingDefaultApplyProbeBoundaryNode>(
              PendingDefaultApplyProbeBoundaryProps(p))
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      this->setLayoutBounds(0, 0, 40, 12);
      this->noteCompositedPaint();
      this->noteOpaquePaintCoverage(true);
      c.declare(loka::app::Text("DefaultApply").testId("PendingDefaultApplyText"));
    }

  protected:
    virtual void applyPendingStructureInfo(const LocalApplyInfo &info, const loka::app::scene::PlatformApplyPlan &plan)
    {
      (void)plan;
      assert(info.hasStructureWork);
      assert(info.isLocalStructureRoot);
      ++g_defaultApplyStructureCalls;
    }

    virtual void applyPendingLayoutInfo(const LocalApplyInfo &info, const loka::app::scene::PlatformApplyPlan &plan)
    {
      (void)plan;
      assert(info.hasLayoutWork);
      assert(info.hasBoundsHint());
      g_defaultApplyBoundsWidth = info.bounds->width;
      g_defaultApplyBoundsHeight = info.bounds->height;
      ++g_defaultApplyLayoutCalls;
    }

    virtual void applyPendingLocalPaintInfo(const LocalApplyInfo &info, const loka::app::scene::PlatformApplyPlan &plan)
    {
      assert(info.hasPaintWork());
      assert(info.hasBoundsHint());
      assert(info.hasPaintSpecificBoundsHint);
      assert(info.usesPaintBoundsHint);
      assert(info.hasPaintBoundsHint());
      assert(info.paintKind == loka::app::scene::BoundaryNode::LOCAL_APPLY_PAINT_GENERIC);
      assert(!plan.isOpaqueLocalPaint());
      ++g_defaultApplyLocalPaintCalls;
    }

    virtual void applyPendingOpaquePaintInfo(const LocalApplyInfo &info,
                                             const loka::app::scene::PlatformApplyPlan &plan)
    {
      assert(info.hasPaintWork());
      assert(info.hasBoundsHint());
      assert(info.hasPaintSpecificBoundsHint);
      assert(info.usesPaintBoundsHint);
      assert(info.hasPaintBoundsHint());
      assert(info.hasOpaqueCoverageHint);
      assert(info.paintKind == loka::app::scene::BoundaryNode::LOCAL_APPLY_PAINT_OPAQUE
             || info.paintKind == loka::app::scene::BoundaryNode::LOCAL_APPLY_PAINT_COMPOSITED);
      assert(info.paintIsOpaque);
      ++g_defaultApplyOpaqueHintSeen;
      ++g_defaultApplyOpaquePaintCalls;
    }

    virtual void applyPendingCompositedPaintInfo(const LocalApplyInfo &info,
                                                 const loka::app::scene::PlatformApplyPlan &plan)
    {
      (void)plan;
      assert(info.hasCompositedPaintWork());
      assert(info.paintKind == loka::app::scene::BoundaryNode::LOCAL_APPLY_PAINT_COMPOSITED);
      ++g_defaultApplyCompositedPaintCalls;
      this->applyPendingOpaquePaintInfo(info, plan);
    }
  };

  class SameBoundaryConditionalProbeNode;
  typedef loka::app::scene::BoundaryPropsFor<SameBoundaryConditionalProbeNode> SameBoundaryConditionalProbeProps;
  static SameBoundaryConditionalProbeNode *g_sameBoundaryConditionalProbe = 0;

  class SameBoundaryConditionalProbeNode : public loka::app::scene::BoundaryNodeFor<SameBoundaryConditionalProbeNode>
  {
  public:
    SameBoundaryConditionalProbeNode(const SameBoundaryConditionalProbeProps &p)
        : loka::app::scene::BoundaryNodeFor<SameBoundaryConditionalProbeNode>(SameBoundaryConditionalProbeProps(p)),
          show_(),
          toggle_(),
          initialized_(false)
    {
    }

    virtual void attachNode(loka::app::scene::NodeComposition &c)
    {
      if (this->initialized_)
      {
        return;
      }
      c.declareStates().state(this->show_, false);
      this->bindForUi(this->toggle_, this, &SameBoundaryConditionalProbeNode::toggle);
      g_sameBoundaryConditionalProbe = this;
      this->initialized_ = true;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      using namespace loka::app;
      TextDefinition falseText = Text("Off").testId("SameBoundaryOffText");
      TextDefinition trueText = Text("On").testId("SameBoundaryOnText");
      c.declare(Box().testId("SameBoundaryRoot") << c.conditional(*this->show_.state(), trueText, falseText));
    }

    void emitToggle()
    {
      this->toggle_.emit();
    }

    void setShown(bool value)
    {
      this->show_.set(value, true);
    }

  private:
    void toggle()
    {
      this->show_.set(!this->show_.get(), true);
    }

    loka::app::scene::NodeState<bool> show_;
    loka::core::EmitterState toggle_;
    bool initialized_;
  };

  class PendingApplySiblingABoundaryNode : public loka::app::scene::BoundaryNodeFor<PendingApplySiblingABoundaryNode>
  {
  public:
    PendingApplySiblingABoundaryNode(const PendingApplySiblingABoundaryProps &p)
        : loka::app::scene::BoundaryNodeFor<PendingApplySiblingABoundaryNode>(PendingApplySiblingABoundaryProps(p))
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      c.declare(loka::app::Text("SiblingA").testId("PendingSiblingAText"));
    }

    virtual bool flushViewDirtyImmediately(loka::app::scene::NodeDirtyFlags) const
    {
      return false;
    }

    virtual void applyPendingUpdate(const loka::app::scene::PlatformApplyPlan &plan)
    {
      const LocalApplyInfo info = this->localApplyInfo(plan);
      ++g_pendingApplySiblingACalls;
      g_pendingApplySiblingALayoutRoot = plan.layoutRoot;
      g_pendingApplySiblingAPaintRoot = plan.paintRoot;
      assert(info.isLocalLayoutRoot || !plan.hasLayoutWork());
      assert(info.isLocalPaintRoot);
      assert(plan.layoutRoot == this);
      assert(plan.paintRoot == this);
    }
  };

  class HeadlessScopeProbeBoundaryNode : public loka::app::scene::BoundaryNodeFor<HeadlessScopeProbeBoundaryNode>
  {
  public:
    HeadlessScopeProbeBoundaryNode(const HeadlessScopeProbeProps &p)
        : loka::app::scene::BoundaryNodeFor<HeadlessScopeProbeBoundaryNode>(HeadlessScopeProbeProps(p)),
          count_(),
          summary_(),
          summaryFlow_(),
          initialized_(false)
    {
    }

    virtual void attachNode(loka::app::scene::NodeComposition &c)
    {
      if (this->initialized_)
      {
        return;
      }
      c.declareStates().state(this->count_, 0).state(this->summary_, loka::core::String::Literal("Headless 0"));
      {
        loka::dsl::StateStream<int> countStream = this->count_.stream();
        this->summaryFlow_.set(countStream.map(loka::dsl::Const("Headless ") + countStream.slot.value()))
            .bindTo(this->summary_);
      }
      ++g_headlessScopeAttachCount;
      g_headlessScopeProbe = this;
      this->initialized_ = true;
    }

    virtual void detachNode(loka::app::scene::NodeComposition &c)
    {
      (void)c;
      ++g_headlessScopeDetachCount;
      if (g_headlessScopeProbe == this)
      {
        g_headlessScopeProbe = 0;
      }
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      c.declare(loka::app::Text(this->summary_.state()).testId("HeadlessSummaryText"));
    }

    void increment()
    {
      this->count_.set(this->count_.get() + 1);
    }

  private:
    loka::app::scene::NodeState<int> count_;
    loka::app::scene::NodeState<loka::core::String> summary_;
    loka::app::scene::FlowSlot<loka::dsl::StateStream<loka::core::String> > summaryFlow_;
    bool initialized_;
  };

  class HeadlessScopeHostBoundaryNode : public loka::app::scene::BoundaryNodeFor<HeadlessScopeHostBoundaryNode>
  {
  public:
    HeadlessScopeHostBoundaryNode(const HeadlessScopeHostProps &p)
        : loka::app::scene::BoundaryNodeFor<HeadlessScopeHostBoundaryNode>(HeadlessScopeHostProps(p)),
          show_(),
          toggle_(),
          initialized_(false)
    {
    }

    virtual void attachNode(loka::app::scene::NodeComposition &c)
    {
      if (this->initialized_)
      {
        return;
      }
      c.declareStates().state(this->show_, true);
      this->bindForUi(this->toggle_, this, &HeadlessScopeHostBoundaryNode::toggle);
      g_headlessScopeHost = this;
      this->initialized_ = true;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      using namespace loka::app;
      c.declare(Box().testId("HeadlessScopeHostRoot")
                << (Show(*this->show_.state()) << loka::app::scene::Boundary<HeadlessScopeProbeBoundaryNode>()));
    }

    void emitToggle()
    {
      this->toggle_.emit();
    }

    void setShown(bool value)
    {
      this->show_.set(value, true);
    }

  private:
    void toggle()
    {
      this->show_.set(!this->show_.get(), true);
    }

    loka::app::scene::NodeState<bool> show_;
    loka::core::EmitterState toggle_;
    bool initialized_;
  };

  class HeadlessOwnedProbeNode : public loka::app::scene::HeadlessNodeBase<HeadlessOwnedProbeProps>
  {
  public:
    HeadlessOwnedProbeNode(const HeadlessOwnedProbeProps &p)
        : loka::app::scene::HeadlessNodeBase<HeadlessOwnedProbeProps>(p),
          count_(),
          summary_(),
          summaryFlow_(),
          initialized_(false)
    {
    }

    virtual ~HeadlessOwnedProbeNode()
    {
      if (g_headlessOwnedProbe == this)
      {
        g_headlessOwnedProbe = 0;
      }
      ++g_headlessOwnedDestroyCount;
    }

    virtual void attachNode(loka::app::scene::NodeComposition &c)
    {
      if (this->initialized_)
      {
        return;
      }
      c.declareStates().state(this->count_, 0).state(this->summary_, loka::core::String::Literal("Owned 0"));
      {
        loka::dsl::StateStream<int> countStream = this->count_.stream();
        this->summaryFlow_.set(countStream.map(loka::dsl::Const("Owned ") + countStream.slot.value()))
            .bindTo(this->summary_);
      }
      ++g_headlessOwnedAttachCount;
      g_headlessOwnedProbe = this;
      this->initialized_ = true;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      c.declare(loka::app::F().testId("HeadlessOwnedSentinel"));
    }

    void increment()
    {
      this->count_.set(this->count_.get() + 1);
    }

    loka::core::String summaryText() const
    {
      return this->summary_.get();
    }

  private:
    loka::app::scene::NodeState<int> count_;
    loka::app::scene::NodeState<loka::core::String> summary_;
    loka::app::scene::FlowSlot<loka::dsl::StateStream<loka::core::String> > summaryFlow_;
    bool initialized_;
  };

  inline loka::app::scene::NodeDefinition<HeadlessOwnedProbeProps, HeadlessOwnedProbeNode> HeadlessOwnedProbe()
  {
    return loka::app::scene::Headless<HeadlessOwnedProbeNode>();
  }

  class HeadlessOwnedHostBoundaryNode : public loka::app::scene::BoundaryNodeFor<HeadlessOwnedHostBoundaryNode>
  {
  public:
    HeadlessOwnedHostBoundaryNode(const HeadlessOwnedHostProps &p)
        : loka::app::scene::BoundaryNodeFor<HeadlessOwnedHostBoundaryNode>(HeadlessOwnedHostProps(p)),
          show_(),
          initialized_(false)
    {
    }

    virtual void attachNode(loka::app::scene::NodeComposition &c)
    {
      if (this->initialized_)
      {
        return;
      }
      c.declareStates().state(this->show_, true);
      g_headlessOwnedHost = this;
      this->initialized_ = true;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      using namespace loka::app;
      c.declare(Box().testId("HeadlessOwnedHostRoot") << (Show(*this->show_.state()) << HeadlessOwnedProbe()));
    }

    void setShown(bool value)
    {
      this->show_.set(value, true);
    }

  private:
    loka::app::scene::NodeState<bool> show_;
    bool initialized_;
  };

  class HeadlessOwnedMultiProbeANode : public loka::app::scene::HeadlessNodeBase<HeadlessOwnedMultiProbeAProps>
  {
  public:
    HeadlessOwnedMultiProbeANode(const HeadlessOwnedMultiProbeAProps &p)
        : loka::app::scene::HeadlessNodeBase<HeadlessOwnedMultiProbeAProps>(p),
          count_(),
          summary_(),
          summaryFlow_(),
          initialized_(false)
    {
    }

    virtual ~HeadlessOwnedMultiProbeANode()
    {
      if (g_headlessOwnedMultiProbeA == this)
      {
        g_headlessOwnedMultiProbeA = 0;
      }
      ++g_headlessOwnedMultiDestroyCountA;
    }

    virtual void attachNode(loka::app::scene::NodeComposition &c)
    {
      if (this->initialized_)
      {
        return;
      }
      c.declareStates().state(this->count_, 0).state(this->summary_, loka::core::String::Literal("OwnedA 0"));
      {
        loka::dsl::StateStream<int> countStream = this->count_.stream();
        this->summaryFlow_.set(countStream.map(loka::dsl::Const("OwnedA ") + countStream.slot.value()))
            .bindTo(this->summary_);
      }
      ++g_headlessOwnedMultiAttachCountA;
      g_headlessOwnedMultiProbeA = this;
      this->initialized_ = true;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      c.declare(loka::app::F().testId("HeadlessOwnedMultiSentinelA"));
    }

    loka::core::String summaryText() const
    {
      return this->summary_.get();
    }

  private:
    loka::app::scene::NodeState<int> count_;
    loka::app::scene::NodeState<loka::core::String> summary_;
    loka::app::scene::FlowSlot<loka::dsl::StateStream<loka::core::String> > summaryFlow_;
    bool initialized_;
  };

  class HeadlessOwnedMultiProbeBNode : public loka::app::scene::HeadlessNodeBase<HeadlessOwnedMultiProbeBProps>
  {
  public:
    HeadlessOwnedMultiProbeBNode(const HeadlessOwnedMultiProbeBProps &p)
        : loka::app::scene::HeadlessNodeBase<HeadlessOwnedMultiProbeBProps>(p),
          count_(),
          summary_(),
          summaryFlow_(),
          initialized_(false)
    {
    }

    virtual ~HeadlessOwnedMultiProbeBNode()
    {
      if (g_headlessOwnedMultiProbeB == this)
      {
        g_headlessOwnedMultiProbeB = 0;
      }
      ++g_headlessOwnedMultiDestroyCountB;
    }

    virtual void attachNode(loka::app::scene::NodeComposition &c)
    {
      if (this->initialized_)
      {
        return;
      }
      c.declareStates().state(this->count_, 0).state(this->summary_, loka::core::String::Literal("OwnedB 0"));
      {
        loka::dsl::StateStream<int> countStream = this->count_.stream();
        this->summaryFlow_.set(countStream.map(loka::dsl::Const("OwnedB ") + countStream.slot.value()))
            .bindTo(this->summary_);
      }
      ++g_headlessOwnedMultiAttachCountB;
      g_headlessOwnedMultiProbeB = this;
      this->initialized_ = true;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      c.declare(loka::app::F().testId("HeadlessOwnedMultiSentinelB"));
    }

    loka::core::String summaryText() const
    {
      return this->summary_.get();
    }

  private:
    loka::app::scene::NodeState<int> count_;
    loka::app::scene::NodeState<loka::core::String> summary_;
    loka::app::scene::FlowSlot<loka::dsl::StateStream<loka::core::String> > summaryFlow_;
    bool initialized_;
  };

  inline loka::app::scene::NodeDefinition<HeadlessOwnedMultiProbeAProps, HeadlessOwnedMultiProbeANode>
  HeadlessOwnedMultiProbeA()
  {
    return loka::app::scene::Headless<HeadlessOwnedMultiProbeANode>();
  }

  inline loka::app::scene::NodeDefinition<HeadlessOwnedMultiProbeBProps, HeadlessOwnedMultiProbeBNode>
  HeadlessOwnedMultiProbeB()
  {
    return loka::app::scene::Headless<HeadlessOwnedMultiProbeBNode>();
  }

  class HeadlessOwnedMultiHostBoundaryNode
      : public loka::app::scene::BoundaryNodeFor<HeadlessOwnedMultiHostBoundaryNode>
  {
  public:
    HeadlessOwnedMultiHostBoundaryNode(const HeadlessOwnedMultiHostProps &p)
        : loka::app::scene::BoundaryNodeFor<HeadlessOwnedMultiHostBoundaryNode>(HeadlessOwnedMultiHostProps(p)),
          show_(),
          initialized_(false)
    {
    }

    virtual void attachNode(loka::app::scene::NodeComposition &c)
    {
      if (this->initialized_)
      {
        return;
      }
      c.declareStates().state(this->show_, true);
      g_headlessOwnedMultiHost = this;
      this->initialized_ = true;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      using namespace loka::app;
      c.declare(Box().testId("HeadlessOwnedMultiHostRoot")
                << (Show(*this->show_.state()) << HeadlessOwnedMultiProbeA() << HeadlessOwnedMultiProbeB()));
    }

    void setShown(bool value)
    {
      this->show_.set(value, true);
    }

  private:
    loka::app::scene::NodeState<bool> show_;
    bool initialized_;
  };

  class HeadlessOwnedPersistentProbeNode : public loka::app::scene::HeadlessNodeBase<HeadlessOwnedPersistentProbeProps>
  {
  public:
    HeadlessOwnedPersistentProbeNode(const HeadlessOwnedPersistentProbeProps &p)
        : loka::app::scene::HeadlessNodeBase<HeadlessOwnedPersistentProbeProps>(p),
          initialized_(false)
    {
    }

    virtual ~HeadlessOwnedPersistentProbeNode()
    {
      if (g_headlessOwnedPersistentProbe == this)
      {
        g_headlessOwnedPersistentProbe = 0;
      }
      ++g_headlessOwnedPersistentDestroyCount;
    }

    virtual void attachNode(loka::app::scene::NodeComposition &c)
    {
      (void)c;
      if (this->initialized_)
      {
        return;
      }
      ++g_headlessOwnedPersistentAttachCount;
      g_headlessOwnedPersistentProbe = this;
      this->initialized_ = true;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      c.declare(loka::app::F().testId("HeadlessOwnedPersistentSentinel"));
    }

  private:
    bool initialized_;
  };

  inline loka::app::scene::NodeDefinition<HeadlessOwnedPersistentProbeProps, HeadlessOwnedPersistentProbeNode>
  HeadlessOwnedPersistentProbe()
  {
    return loka::app::scene::Headless<HeadlessOwnedPersistentProbeNode>();
  }

  class HeadlessOwnedMixedHostBoundaryNode
      : public loka::app::scene::BoundaryNodeFor<HeadlessOwnedMixedHostBoundaryNode>
  {
  public:
    HeadlessOwnedMixedHostBoundaryNode(const HeadlessOwnedMixedHostProps &p)
        : loka::app::scene::BoundaryNodeFor<HeadlessOwnedMixedHostBoundaryNode>(HeadlessOwnedMixedHostProps(p)),
          show_(),
          initialized_(false)
    {
    }

    virtual void attachNode(loka::app::scene::NodeComposition &c)
    {
      if (this->initialized_)
      {
        return;
      }
      c.declareStates().state(this->show_, true);
      this->initialized_ = true;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      using namespace loka::app;
      c.declare(Box().testId("HeadlessOwnedMixedHostRoot")
                << HeadlessOwnedPersistentProbe() << (Show(*this->show_.state()) << HeadlessOwnedProbe()));
    }

    void setShown(bool value)
    {
      this->show_.set(value, true);
    }

  private:
    loka::app::scene::NodeState<bool> show_;
    bool initialized_;
  };

  class PendingApplySiblingBBoundaryNode : public loka::app::scene::BoundaryNodeFor<PendingApplySiblingBBoundaryNode>
  {
  public:
    PendingApplySiblingBBoundaryNode(const PendingApplySiblingBBoundaryProps &p)
        : loka::app::scene::BoundaryNodeFor<PendingApplySiblingBBoundaryNode>(PendingApplySiblingBBoundaryProps(p))
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      c.declare(loka::app::Text("SiblingB").testId("PendingSiblingBText"));
    }

    virtual bool flushViewDirtyImmediately(loka::app::scene::NodeDirtyFlags) const
    {
      return false;
    }

    virtual void applyPendingUpdate(const loka::app::scene::PlatformApplyPlan &plan)
    {
      const LocalApplyInfo info = this->localApplyInfo(plan);
      ++g_pendingApplySiblingBCalls;
      g_pendingApplySiblingBLayoutRoot = plan.layoutRoot;
      g_pendingApplySiblingBPaintRoot = plan.paintRoot;
      assert(info.isLocalLayoutRoot || !plan.hasLayoutWork());
      assert(info.isLocalPaintRoot);
      assert(plan.layoutRoot == this);
      assert(plan.paintRoot == this);
    }
  };

  class PendingApplySiblingsRootNode : public loka::app::scene::BoundaryNodeFor<PendingApplySiblingsRootNode>
  {
  public:
    PendingApplySiblingsRootNode(const PendingApplySiblingsRootProps &p)
        : loka::app::scene::BoundaryNodeFor<PendingApplySiblingsRootNode>(PendingApplySiblingsRootProps(p))
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      c.declare(
          loka::app::VStack()
          << loka::app::scene::BoundaryDefinition<PendingApplySiblingABoundaryProps, PendingApplySiblingABoundaryNode>()
                 .tag(101)
          << loka::app::scene::BoundaryDefinition<PendingApplySiblingBBoundaryProps, PendingApplySiblingBBoundaryNode>()
                 .tag(102));
    }
  };

  class PendingApplyDefersSiblingABoundaryNode
      : public loka::app::scene::BoundaryNodeFor<PendingApplyDefersSiblingABoundaryNode>
  {
  public:
    PendingApplyDefersSiblingABoundaryNode(const PendingApplyDefersSiblingABoundaryProps &p)
        : loka::app::scene::BoundaryNodeFor<PendingApplyDefersSiblingABoundaryNode>(
              PendingApplyDefersSiblingABoundaryProps(p))
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      c.declare(loka::app::Text("DeferA").testId("PendingDeferSiblingAText"));
    }

    virtual bool flushViewDirtyImmediately(loka::app::scene::NodeDirtyFlags) const
    {
      return false;
    }

    virtual void applyPendingUpdate(const loka::app::scene::PlatformApplyPlan &plan)
    {
      ++g_pendingApplyDefersSiblingACalls;
      assert(plan.paintRoot == this);
      if (!g_pendingApplyDefersSiblingAQueuedSiblingB)
      {
        g_pendingApplyDefersSiblingAQueuedSiblingB = true;
        assert(this->isApplyingPlatform());
        assert(g_pendingApplyDeferredSiblingB != 0);
        g_pendingApplyDeferredSiblingB->markViewDirty(loka::app::scene::NODE_DIRTY_PROPS);
      }
    }
  };

  class PendingApplyDefersSiblingBBoundaryNode
      : public loka::app::scene::BoundaryNodeFor<PendingApplyDefersSiblingBBoundaryNode>
  {
  public:
    PendingApplyDefersSiblingBBoundaryNode(const PendingApplyDefersSiblingBBoundaryProps &p)
        : loka::app::scene::BoundaryNodeFor<PendingApplyDefersSiblingBBoundaryNode>(
              PendingApplyDefersSiblingBBoundaryProps(p))
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      c.declare(loka::app::Text("DeferB").testId("PendingDeferSiblingBText"));
    }

    virtual bool flushViewDirtyImmediately(loka::app::scene::NodeDirtyFlags) const
    {
      return false;
    }

    virtual void applyPendingUpdate(const loka::app::scene::PlatformApplyPlan &plan)
    {
      ++g_pendingApplyDefersSiblingBCalls;
      assert(plan.paintRoot == this);
    }
  };

  class PendingApplyDefersSiblingsRootNode
      : public loka::app::scene::BoundaryNodeFor<PendingApplyDefersSiblingsRootNode>
  {
  public:
    PendingApplyDefersSiblingsRootNode(const PendingApplyDefersSiblingsRootProps &p)
        : loka::app::scene::BoundaryNodeFor<PendingApplyDefersSiblingsRootNode>(
              PendingApplyDefersSiblingsRootProps(p))
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      c.declare(
          loka::app::VStack()
          << loka::app::scene::BoundaryDefinition<PendingApplyDefersSiblingABoundaryProps,
                                                  PendingApplyDefersSiblingABoundaryNode>()
                 .tag(201)
          << loka::app::scene::BoundaryDefinition<PendingApplyDefersSiblingBBoundaryProps,
                                                  PendingApplyDefersSiblingBBoundaryNode>()
                 .tag(202));
    }
  };

  struct FlowTestMarker
  {
    static void onStepSuccess(const int &, void *user)
    {
      FlowTestMarkerContext *ctx = static_cast<FlowTestMarkerContext *>(user);
      ctx->order->push_back(ctx->marker);
    }

    static void onStepFinally(void *user)
    {
      FlowTestMarkerContext *ctx = static_cast<FlowTestMarkerContext *>(user);
      ctx->order->push_back(ctx->marker);
    }

    static void onFlowSuccess(void *user)
    {
      FlowTestMarkerContext *ctx = static_cast<FlowTestMarkerContext *>(user);
      ctx->order->push_back(ctx->marker);
    }

    static loka::dsl::FlowHandleResult onFlowFailureHandled(const loka::dsl::FlowError &, void *user)
    {
      FlowTestMarkerContext *ctx = static_cast<FlowTestMarkerContext *>(user);
      ctx->order->push_back(ctx->marker);
      return loka::dsl::FLOW_ERROR_HANDLED;
    }

    static loka::dsl::FlowHandleResult onStepFailureUnhandled(const loka::dsl::FlowError &, void *user)
    {
      FlowTestMarkerContext *ctx = static_cast<FlowTestMarkerContext *>(user);
      ctx->order->push_back(ctx->marker);
      return loka::dsl::FLOW_ERROR_UNHANDLED;
    }

    static loka::dsl::FlowHandleResult onStepFailureHandled(const loka::dsl::FlowError &, void *user)
    {
      FlowTestMarkerContext *ctx = static_cast<FlowTestMarkerContext *>(user);
      ctx->order->push_back(ctx->marker);
      return loka::dsl::FLOW_ERROR_HANDLED;
    }

    static bool is500(const loka::dsl::FlowError &error, void *)
    {
      return error.code == 500;
    }

    static loka::dsl::FlowHandleResult captureFailure(const loka::dsl::FlowError &error, void *user)
    {
      FlowErrorCapture *capture = static_cast<FlowErrorCapture *>(user);
      if (capture)
      {
        capture->kind = error.kind;
        capture->code = error.code;
        ++capture->calls;
      }
      return loka::dsl::FLOW_ERROR_HANDLED;
    }
  };

  struct FlowTestMul2Adapter
  {
    typedef int In;
    typedef int Out;
    loka::dsl::StepRunStatus run(const int &in, int &out, loka::dsl::FlowError &) const
    {
      out = in * 2;
      return loka::dsl::FLOW_STEP_SUCCEEDED;
    }
  };

  struct FlowTestAdd1Adapter
  {
    typedef int In;
    typedef int Out;
    loka::dsl::StepRunStatus run(const int &in, int &out, loka::dsl::FlowError &) const
    {
      out = in + 1;
      return loka::dsl::FLOW_STEP_SUCCEEDED;
    }
  };

  struct FlowTestFail500Adapter
  {
    typedef int In;
    typedef int Out;
    loka::dsl::StepRunStatus run(const int &, int &out, loka::dsl::FlowError &error) const
    {
      out = 0;
      error.kind = 1;
      error.code = 500;
      return loka::dsl::FLOW_STEP_FAILED;
    }
  };

  struct FlowTestPredicates
  {
    static bool greaterThan10(const int &value, void *)
    {
      return value > 10;
    }

    static bool equalsExpected(const int &value, void *user)
    {
      const int *expected = static_cast<const int *>(user);
      return expected != 0 && value == *expected;
    }
  };

  struct FlowTestCheckLoadingAdapter
  {
    typedef int In;
    typedef int Out;

    explicit FlowTestCheckLoadingAdapter(const bool *loadingState)
        : loadingState_(loadingState)
    {
    }

    loka::dsl::StepRunStatus run(const int &in, int &out, loka::dsl::FlowError &) const
    {
      assert(this->loadingState_ != 0);
      assert(*this->loadingState_ == true);
      out = in;
      return loka::dsl::FLOW_STEP_SUCCEEDED;
    }

    const bool *loadingState_;
  };

  struct FlowTestPendingThenSuccessAdapter
  {
    typedef int In;
    typedef int Out;

    FlowTestPendingThenSuccessAdapter(bool *ready, int *calls)
        : ready_(ready),
          calls_(calls)
    {
    }

    loka::dsl::StepRunStatus run(const int &in, int &out, loka::dsl::FlowError &) const
    {
      ++(*this->calls_);
      if (!*this->ready_)
      {
        return loka::dsl::FLOW_STEP_PENDING;
      }
      out = in + 100;
      return loka::dsl::FLOW_STEP_SUCCEEDED;
    }

    bool *ready_;
    int *calls_;
  };

  struct FlowTestFailOnceAdapter
  {
    typedef int In;
    typedef int Out;

    explicit FlowTestFailOnceAdapter(bool *failedOnce)
        : failedOnce_(failedOnce)
    {
    }

    loka::dsl::StepRunStatus run(const int &in, int &out, loka::dsl::FlowError &error) const
    {
      if (!*this->failedOnce_)
      {
        *this->failedOnce_ = true;
        error.kind = 9;
        error.code = 500;
        return loka::dsl::FLOW_STEP_FAILED;
      }
      out = in + 7;
      return loka::dsl::FLOW_STEP_SUCCEEDED;
    }

    bool *failedOnce_;
  };

  struct FlowTestCountedPassAdapter
  {
    typedef int In;
    typedef int Out;

    explicit FlowTestCountedPassAdapter(int *calls)
        : calls_(calls)
    {
    }

    loka::dsl::StepRunStatus run(const int &in, int &out, loka::dsl::FlowError &) const
    {
      ++(*this->calls_);
      out = in + 1;
      return loka::dsl::FLOW_STEP_SUCCEEDED;
    }

    int *calls_;
  };

  struct FlowTestFieldOutput
  {
    int value;
    int extra;
    FlowTestFieldOutput()
        : value(0),
          extra(0)
    {
    }
  };

  struct FlowTestFieldAdapter
  {
    typedef int In;
    typedef FlowTestFieldOutput Out;
    loka::dsl::StepRunStatus run(const int &in, FlowTestFieldOutput &out, loka::dsl::FlowError &) const
    {
      out.value = in * 10;
      out.extra = in + 1;
      return loka::dsl::FLOW_STEP_SUCCEEDED;
    }
  };

  struct FlowTestSnapTextValueAdapter
  {
    typedef int In;
    typedef loka::dsl::SnapRecord Out;

    FlowTestSnapTextValueAdapter(const char *stepName, const char *value, long tick)
        : stepName_(stepName ? stepName : ""),
          value_(value ? value : ""),
          tick_(tick)
    {
    }

    loka::dsl::StepRunStatus run(const int &, loka::dsl::SnapRecord &out, loka::dsl::FlowError &) const
    {
      out.set("test", "SceneFlow");
      out.set("step", this->stepName_.c_str());
      out.set("node", "MainText");
      out.setInt("tick", this->tick_);
      out.setInt("scenario_version", 1);
      out.set("status", loka::dsl::SnapStatusOk());
      out.set("text.value", this->value_.c_str());
      return loka::dsl::FLOW_STEP_SUCCEEDED;
    }

    std::string stepName_;
    std::string value_;
    long tick_;
  };

  struct FlowTestSnapDirtyMaskAdapter
  {
    typedef int In;
    typedef loka::dsl::SnapRecord Out;

    FlowTestSnapDirtyMaskAdapter(const char *stepName, long dirtyMask, long tick)
        : stepName_(stepName ? stepName : ""),
          dirtyMask_(dirtyMask),
          tick_(tick)
    {
    }

    loka::dsl::StepRunStatus run(const int &, loka::dsl::SnapRecord &out, loka::dsl::FlowError &) const
    {
      out.set("test", "SceneFlow");
      out.set("step", this->stepName_.c_str());
      out.set("node", "MainText");
      out.setInt("tick", this->tick_);
      out.setInt("scenario_version", 1);
      out.set("status", loka::dsl::SnapStatusOk());
      out.setInt("dirty.mask", this->dirtyMask_);
      return loka::dsl::FLOW_STEP_SUCCEEDED;
    }

    std::string stepName_;
    long dirtyMask_;
    long tick_;
  };

  struct FlowTestPlatformContext : public PlatformContext
  {
    FlowTestPlatformContext()
        : createImageResult_(false),
          createImageCalls_(0),
          width_(16),
          height_(16)
    {
    }

    virtual App *createApp(AppConfigurable *, HINSTANCE, int) const
    {
      return 0;
    }
    virtual Window *createWindow(const WindowProps &)
    {
      return 0;
    }
    virtual loka::app::scene::NodeContext *createNodeContext(loka::app::scene::Node *) const
    {
      return 0;
    }
    virtual bool openFile(const loka::file::File &, loka::platform::file::FileHandle &) const
    {
      return false;
    }
    virtual bool createImageFromBlob(const loka::core::resource::Blob &, loka::core::resource::Image &out) const
    {
      ++createImageCalls_;
      if (!createImageResult_)
      {
        return false;
      }
      out = loka::core::resource::Image::FromNative(
          reinterpret_cast<void *>(0x1), width_, height_, &FlowTestPlatformContext::ReleaseNativeNoop, 0);
      return true;
    }

    static void ReleaseNativeNoop(void *, void *) {}

    bool createImageResult_;
    mutable int createImageCalls_;
    int width_;
    int height_;
  };

  struct FlowTestCapturableBitmap : public loka::app::scene::ICapturableBitmap
  {
    virtual bool captureBitmap(loka::core::resource::Image &out) const
    {
      out = loka::core::resource::Image::FromNative(
          reinterpret_cast<void *>(0x2), 80, 40, &FlowTestPlatformContext::ReleaseNativeNoop, 0);
      return out.isValid();
    }
  };

  struct FlowTestCapturableNodeContext : public loka::app::scene::NodeContext
  {
    FlowTestCapturableNodeContext()
        : bitmap_()
    {
    }

    virtual loka::app::scene::ICapturableBitmap *asCapturableBitmap()
    {
      return &bitmap_;
    }
    virtual const loka::app::scene::ICapturableBitmap *asCapturableBitmap() const
    {
      return &bitmap_;
    }

  private:
    FlowTestCapturableBitmap bitmap_;
  };

  struct StateNotifyDeleteCtx
  {
    loka::core::EmitterState *state;
    loka::core::MutableState<int> *valueState;
    int destroyCalls;
    int siblingCalls;
  };

  struct StateNotifyHelpers
  {
    static void destroySelf(void *user)
    {
      StateNotifyDeleteCtx *ctx = static_cast<StateNotifyDeleteCtx *>(user);
      ++ctx->destroyCalls;
      if (ctx->state)
      {
        loka::core::EmitterState *owned = ctx->state;
        ctx->state = 0;
        delete owned;
      }
    }

    static void sibling(void *user)
    {
      StateNotifyDeleteCtx *ctx = static_cast<StateNotifyDeleteCtx *>(user);
      ++ctx->siblingCalls;
    }

    static void destroyValueSelf(void *user)
    {
      StateNotifyDeleteCtx *ctx = static_cast<StateNotifyDeleteCtx *>(user);
      ++ctx->destroyCalls;
      if (ctx->valueState)
      {
        loka::core::MutableState<int> *owned = ctx->valueState;
        ctx->valueState = 0;
        delete owned;
      }
    }

    static void selfUnbind(void *user)
    {
      StateNotifyDeleteCtx *ctx = static_cast<StateNotifyDeleteCtx *>(user);
      ++ctx->destroyCalls;
      if (ctx->state)
      {
        ctx->state->unbind(&StateNotifyHelpers::selfUnbind, user);
      }
    }

    static void unbindSibling(void *user)
    {
      StateNotifyDeleteCtx *ctx = static_cast<StateNotifyDeleteCtx *>(user);
      ++ctx->destroyCalls;
      if (ctx->state)
      {
        ctx->state->unbind(&StateNotifyHelpers::sibling, user);
      }
    }

    static void unbindValueSibling(void *user)
    {
      StateNotifyDeleteCtx *ctx = static_cast<StateNotifyDeleteCtx *>(user);
      ++ctx->destroyCalls;
      if (ctx->valueState)
      {
        ctx->valueState->unbind(&StateNotifyHelpers::sibling, user);
      }
    }
  };

  struct FlowScenePlatformController : public loka::app::scene::IPlatformController
  {
    FlowScenePlatformController()
        : lastMaterialized_(0),
          lastFlags_(loka::app::scene::NODE_DIRTY_NONE),
          lastFullRebuild_(false),
          lastBoundaryApplyRoot_(0),
          lastBoundaryApplyBoundary_(0),
          lastBoundaryApplyInfo_(),
          lastBoundaryApplyPlan_(),
          boundaryApplyCalls_(0),
          calls_(0),
          skipGlobalChangeForBoundaryLocalPaint_(false),
          destroyed_(false)
    {
    }

    virtual void onChange(loka::app::scene::Node *rootNode, loka::app::scene::NodeDirtyFlags flags, bool fullRebuild)
    {
      lastMaterialized_ = rootNode;
      lastFlags_ = flags;
      lastFullRebuild_ = fullRebuild;
      ++calls_;
    }

    virtual void onBoundaryApply(loka::app::scene::Node *rootNode,
                                 loka::app::scene::BoundaryNode *boundary,
                                 const loka::app::scene::BoundaryLocalApplyInfo &info,
                                 const loka::app::scene::PlatformApplyPlan &plan)
    {
      assert(plan.isLocalizedFor(boundary));
      assert(plan.hasBoundaryApplyWork(boundary));
      lastBoundaryApplyRoot_ = rootNode;
      lastBoundaryApplyBoundary_ = boundary;
      lastBoundaryApplyInfo_ = info;
      lastBoundaryApplyPlan_ = plan;
      ++boundaryApplyCalls_;
    }

    virtual bool canSkipGlobalChangeForBoundaryLocalPaint() const
    {
      return skipGlobalChangeForBoundaryLocalPaint_;
    }

    virtual void synchronize() {}

    virtual bool hasPendingSync() const
    {
      return false;
    }

    virtual void destroy()
    {
      destroyed_ = true;
    }

    loka::app::scene::Node *lastMaterialized_;
    loka::app::scene::NodeDirtyFlags lastFlags_;
    bool lastFullRebuild_;
    loka::app::scene::Node *lastBoundaryApplyRoot_;
    loka::app::scene::BoundaryNode *lastBoundaryApplyBoundary_;
    loka::app::scene::BoundaryLocalApplyInfo lastBoundaryApplyInfo_;
    loka::app::scene::PlatformApplyPlan lastBoundaryApplyPlan_;
    int boundaryApplyCalls_;
    int calls_;
    bool skipGlobalChangeForBoundaryLocalPaint_;
    bool destroyed_;
  };

  struct FlowTestPlatformDirtyMaskAdapter
  {
    typedef loka::app::scene::Scene *In;
    typedef loka::dsl::SnapRecord Out;

    FlowTestPlatformDirtyMaskAdapter(const char *stepName, const FlowScenePlatformController *platform, long tick)
        : stepName_(stepName ? stepName : ""),
          platform_(platform),
          tick_(tick)
    {
    }

    loka::dsl::StepRunStatus
    run(loka::app::scene::Scene *const &, loka::dsl::SnapRecord &out, loka::dsl::FlowError &error) const
    {
      if (!this->platform_)
      {
        error.kind = loka::dsl::testing::FLOW_ERROR_KIND_SCENE_SCENARIO;
        error.code = loka::dsl::testing::FLOW_ERROR_SCENE_TEST_NULL_SCENE;
        return loka::dsl::FLOW_STEP_FAILED;
      }
      out.set("test", "SceneFlow");
      out.set("step", this->stepName_.c_str());
      out.set("node", "ScenePlatform");
      out.setInt("tick", this->tick_);
      out.setInt("scenario_version", 1);
      out.set("status", loka::dsl::SnapStatusOk());
      out.setInt("platform.dirty.mask", static_cast<long>(this->platform_->lastFlags_));
      out.setInt("platform.materialized", this->platform_->lastMaterialized_ ? 1 : 0);
      out.setInt("platform.full_rebuild", this->platform_->lastFullRebuild_ ? 1 : 0);
      out.setInt("platform.calls", this->platform_->calls_);
      return loka::dsl::FLOW_STEP_SUCCEEDED;
    }

    std::string stepName_;
    const FlowScenePlatformController *platform_;
    long tick_;
  };

  struct FlowTestViewBitmapCaptureHelpers
  {
    static bool capture(const loka::dsl::testing::ViewCaptureTarget &target,
                        loka::dsl::testing::ViewBitmapCapture &out,
                        loka::dsl::FlowError &error,
                        void *)
    {
      if (!target.node)
      {
        error.kind = loka::dsl::testing::FLOW_ERROR_KIND_SCENE_SCENARIO;
        error.code = loka::dsl::testing::FLOW_ERROR_SCENE_TEST_NODE_NOT_FOUND;
        return false;
      }
      out.image = loka::core::resource::Image::FromNative(
          reinterpret_cast<void *>(0x1), 64, 32, &FlowTestPlatformContext::ReleaseNativeNoop, 0);
      out.captured = out.image.isValid();
      out.width = out.image.width();
      out.height = out.image.height();
      return out.captured;
    }
  };
} // namespace

void testLokaFlowDslV1Core()
{
  printf("\n==== [testLokaFlowDslV1Core] start ====\n");

  {
    using namespace loka::app::scene;

    Node node;
    assert(node.resolveChildComposeEvent(COMPOSE_EVENT_UPDATE) == COMPOSE_EVENT_UPDATE);
    node.markPendingAttachForCompose();
    assert(node.resolveChildComposeEvent(COMPOSE_EVENT_UPDATE) == COMPOSE_EVENT_ATTACH);
    assert(node.resolveChildComposeEvent(COMPOSE_EVENT_UPDATE) == COMPOSE_EVENT_UPDATE);
    node.markPendingAttachForCompose();
    assert(node.resolveChildComposeEvent(COMPOSE_EVENT_ATTACH) == COMPOSE_EVENT_ATTACH);
    assert(node.resolveChildComposeEvent(COMPOSE_EVENT_DETACH) == COMPOSE_EVENT_DETACH);
  }

  {
    loka::app::HStack buttons = buildTypedHStack();
    assert(buttons.childrenCount() == 2);
    assert(buttons.props.hasVerticalAlignment_);
    assert(buttons.props.verticalAlignment_ == loka::app::VERTICAL_ALIGNMENT_BOTTOM);
  }

  {
    loka::app::VStack column = buildTypedVStack();
    assert(column.childrenCount() == 2);
    assert(column.props.hasHorizontalAlignment_);
    assert(column.props.horizontalAlignment_ == loka::app::HORIZONTAL_ALIGNMENT_CENTER);
  }

  {
    loka::app::Box box = buildTypedBox();
    assert(box.childrenCount() == 1);
    assert(box.props.padding == 8);
    assert(box.hasTestId());
    assert(box.testIdValue() == "TypedBox");
  }

  {
    loka::app::Fragment fragment = buildTypedFragment();
    assert(fragment.childrenCount() == 2);
  }

  {
    loka::app::Grid grid = buildTypedGrid();
    assert(grid.childrenCount() == 2);
    assert(grid.props.rows == 1);
    assert(grid.props.cols == 2);
  }

  {
    loka::app::ZStack stack = buildTypedZStack();
    assert(stack.childrenCount() == 2);
    assert(stack.hasTestId());
    assert(stack.testIdValue() == "TypedZStack");
  }

  {
    using namespace loka::app::scene;

    Scene scene(BoundaryDefinition<TypedDslLightHostProps, TypedDslLightHostNode>().clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    loka::app::TextNode *lightChild = 0;
    loka::dsl::FlowError lookupError;
    assert(
        loka::dsl::testing::LookupNodeById<loka::app::TextNode>(&scene, "TypedDslLightChild", lightChild, lookupError)
        != loka::dsl::FLOW_STEP_FAILED);
    assert(lightChild != 0);

    scene.unmount();
  }

  {
    RecordingDirtySourceRegistrar registrar;
    loka::app::TextNode constantTextNode(loka::app::TextProps("Constant text"));
    constantTextNode.declareDirtySources(registrar);
    assert(registrar.calls == 0);

    loka::core::MutableState<loka::core::String> liveText(loka::core::String::Literal("Live text"));
    loka::app::TextNode liveTextNode((loka::app::TextProps(&liveText)));
    liveTextNode.declareDirtySources(registrar);
    assert(registrar.calls == 1);
    assert(registrar.lastState == &liveText);
    assert(registrar.lastFlags == loka::app::scene::NODE_DIRTY_PROPS);
  }

  {
    RecordingDirtySourceRegistrar registrar;
    loka::app::ButtonProps constantButtonProps;
    constantButtonProps.text("Constant button");
    loka::app::ButtonNode constantButtonNode(constantButtonProps);
    constantButtonNode.declareDirtySources(registrar);
    assert(registrar.calls == 0);

    loka::core::MutableState<loka::core::String> liveButtonText(loka::core::String::Literal("Live button"));
    loka::app::ButtonProps liveButtonProps;
    liveButtonProps.text(&liveButtonText);
    loka::app::ButtonNode liveButtonNode(liveButtonProps);
    liveButtonNode.declareDirtySources(registrar);
    assert(registrar.calls == 1);
    assert(registrar.lastState == &liveButtonText);
    assert(registrar.lastFlags == loka::app::scene::NODE_DIRTY_PROPS);
  }

  {
    RecordingDirtySourceRegistrar registrar;
    loka::app::CellProps constantCellProps;
    constantCellProps.text("Constant cell");
    loka::app::CellNode constantCellNode(constantCellProps);
    constantCellNode.declareDirtySources(registrar);
    assert(registrar.calls == 0);

    loka::core::MutableState<loka::core::String> liveCellText(loka::core::String::Literal("Live cell"));
    loka::app::CellProps liveCellProps;
    liveCellProps.text(&liveCellText);
    loka::app::CellNode liveCellNode(liveCellProps);
    liveCellNode.declareDirtySources(registrar);
    assert(registrar.calls == 1);
    assert(registrar.lastState == &liveCellText);
    assert(registrar.lastFlags == loka::app::scene::NODE_DIRTY_PROPS);
  }

  {
    PendingLayoutBoundaryProps::assertAllowedValueType<int>();
    PendingLayoutBoundaryProps::assertAllowedValueType<loka::core::State<int> *>();
    PendingLayoutBoundaryProps::assertAllowedValueType<loka::core::Managed<loka::core::MutableState<int> > >();

    assert((loka::app::scene::BoundaryPropValueRules<int>::kAllowed));
    assert((loka::app::scene::BoundaryPropValueRules<loka::core::State<int> *>::kAllowed));
    assert((loka::app::scene::BoundaryPropValueRules<loka::core::Managed<loka::core::MutableState<int> > >::kAllowed));
    assert(!(loka::app::scene::BoundaryPropValueRules<loka::app::scene::NodeState<int> >::kAllowed));
    assert(!(loka::app::scene::BoundaryPropValueRules<loka::core::MutableState<int> *>::kAllowed));
    loka::core::MutableState<int> countState(21);
    loka::app::scene::BorrowedState<int> borrowedCount = PendingLayoutBoundaryProps::borrowed<int>(&countState);
    loka::core::Managed<loka::core::MutableState<int> > sharedCount = PendingLayoutBoundaryProps::shared(
        loka::core::Managed<loka::core::MutableState<int> >::Wrap(new loka::core::MutableState<int>(34)));
    assert(borrowedCount.isValid());
    assert(borrowedCount.get() == 21);
    assert(sharedCount.get() != 0);
    assert(sharedCount.get()->get() == 34);
  }

  {
    loka::app::scene::BorrowedState<int> emptyBorrowed;
    assert(!emptyBorrowed.isValid());
  }

  {
    loka::app::scene::NodeComposition composition;
    loka::app::scene::ComponentContext context;

    context.setBoundary(reinterpret_cast<loka::app::scene::BoundaryNode *>(0x6100));
    composition.setContext(&context);

    const loka::app::scene::NodeComposition::CurrentBoundary current = composition.currentBoundary();
    assert(!current.isValid());
  }

  {
    loka::app::scene::NodeComposition composition;
    loka::app::scene::ComponentContext parentBoundaryContext;
    loka::app::scene::ComponentContext currentBoundaryContext(&parentBoundaryContext);
    loka::app::scene::ComponentContext childContext(&currentBoundaryContext);
    BoundaryLookupTestNode parentBoundaryNode(2);
    loka::app::scene::Node currentBoundaryNode;
    loka::app::scene::Node childNode;

    parentBoundaryContext.setOwner(&parentBoundaryNode);
    parentBoundaryContext.setBoundary(reinterpret_cast<loka::app::scene::BoundaryNode *>(0x2000));
    currentBoundaryContext.setOwner(&currentBoundaryNode);
    currentBoundaryContext.setBoundary(reinterpret_cast<loka::app::scene::BoundaryNode *>(0x3000));
    childContext.setOwner(&childNode);
    childContext.setBoundary(reinterpret_cast<loka::app::scene::BoundaryNode *>(0x3000));
    composition.setContext(&childContext);

    loka::app::scene::NodeComposition::FoundBoundary<BoundaryLookupTestApi> foundParent =
        composition.findBoundary<BoundaryLookupTestApi>();
    assert(foundParent.isValid());
    assert(foundParent.facade().id() == 2);
  }

  {
    loka::app::scene::NodeComposition composition;
    loka::app::scene::ComponentContext context;
    BoundaryLookupTestNode currentBoundaryNode(4);

    context.setOwner(&currentBoundaryNode);
    context.setBoundary(reinterpret_cast<loka::app::scene::BoundaryNode *>(0x2050));
    composition.setContext(&context);

    const loka::app::scene::NodeComposition::FoundBoundary<BoundaryLookupTestApi> foundParent =
        composition.findBoundary<BoundaryLookupTestApi>();
    assert(!foundParent.isValid());
  }

  {
    loka::app::scene::NodeComposition composition;
    loka::app::scene::ComponentContext parentBoundaryContext;
    loka::app::scene::ComponentContext currentBoundaryContext(&parentBoundaryContext);
    loka::app::scene::ComponentContext childContext(&currentBoundaryContext);
    BoundaryLookupStateNode parentBoundaryNode(9);
    loka::app::scene::Node currentBoundaryNode;
    loka::app::scene::Node childNode;

    parentBoundaryContext.setOwner(&parentBoundaryNode);
    parentBoundaryContext.setBoundary(reinterpret_cast<loka::app::scene::BoundaryNode *>(0x2100));
    currentBoundaryContext.setOwner(&currentBoundaryNode);
    currentBoundaryContext.setBoundary(reinterpret_cast<loka::app::scene::BoundaryNode *>(0x3100));
    childContext.setOwner(&childNode);
    childContext.setBoundary(reinterpret_cast<loka::app::scene::BoundaryNode *>(0x3100));
    composition.setContext(&childContext);

    const loka::app::scene::NodeComposition::FoundBoundary<BoundaryLookupStateApi> foundParent =
        composition.findBoundary<BoundaryLookupStateApi>();
    assert(foundParent.isValid());
    assert(foundParent.facade().countState().isValid());
    assert(foundParent.facade().countState().get() == 9);
  }

  {
    loka::app::scene::NodeComposition composition;
    loka::app::scene::ComponentContext grandparentContext;
    loka::app::scene::ComponentContext parentBoundaryContext(&grandparentContext);
    loka::app::scene::ComponentContext currentBoundaryContext(&parentBoundaryContext);
    loka::app::scene::ComponentContext childContext(&currentBoundaryContext);
    BoundaryLookupTestNode grandparentNode(3);
    loka::app::scene::Node parentBoundaryNode;
    loka::app::scene::Node currentBoundaryNode;
    loka::app::scene::Node childNode;

    grandparentContext.setOwner(&grandparentNode);
    grandparentContext.setBoundary(reinterpret_cast<loka::app::scene::BoundaryNode *>(0x3000));
    parentBoundaryContext.setOwner(&parentBoundaryNode);
    parentBoundaryContext.setBoundary(reinterpret_cast<loka::app::scene::BoundaryNode *>(0x4000));
    currentBoundaryContext.setOwner(&currentBoundaryNode);
    currentBoundaryContext.setBoundary(reinterpret_cast<loka::app::scene::BoundaryNode *>(0x5000));
    childContext.setOwner(&childNode);
    childContext.setBoundary(reinterpret_cast<loka::app::scene::BoundaryNode *>(0x5000));
    composition.setContext(&childContext);

    loka::app::scene::NodeComposition::FoundBoundary<BoundaryLookupTestApi> foundGrandparent =
        composition.findBoundary<BoundaryLookupTestApi>();
    assert(!foundGrandparent.isValid());
  }

  {
    DummyStateOwner owner;
    loka::app::scene::NodeComposition composition;
    loka::app::scene::ComponentContext context;
    loka::core::MutableState<int> ownedValue(3);
    loka::app::scene::NodeState<int> nodeState(&ownedValue, 0, &owner);

    context.setBoundary(reinterpret_cast<loka::app::scene::BoundaryNode *>(0x6000));
    context.setStateOwner(&owner);
    composition.setContext(&context);

    loka::app::scene::NodeComposition::CurrentBoundary current = composition.currentBoundary();
    loka::app::scene::NodeComposition::CurrentBoundary::CurrentState<int> foundState = current.state(nodeState);
    assert(current.isValid());
    assert(foundState.isValid());
    assert(foundState.get() == 3);
    foundState.set(7);
    assert(ownedValue.get() == 7);
  }

  {
    DummyStateOwner currentOwner;
    DummyStateOwner foreignOwner;
    loka::app::scene::NodeComposition composition;
    loka::app::scene::ComponentContext context;
    loka::core::MutableState<int> foreignValue(11);
    loka::app::scene::NodeState<int> foreignNodeState(&foreignValue, 0, &foreignOwner);

    context.setBoundary(reinterpret_cast<loka::app::scene::BoundaryNode *>(0x7000));
    context.setStateOwner(&currentOwner);
    composition.setContext(&context);

    loka::app::scene::NodeComposition::CurrentBoundary::CurrentState<int> foundForeignState =
        composition.currentBoundary().state(foreignNodeState);
    assert(!foundForeignState.isValid());
  }

  {
    int input = 3;
    int captured = 0;
    std::vector<int> order;

    FlowTestMarkerContext s1 = {&order, 10};
    FlowTestMarkerContext f1 = {&order, 11};
    FlowTestMarkerContext fs1 = {&order, 12};
    FlowTestMarkerContext s2 = {&order, 20};
    FlowTestMarkerContext f2 = {&order, 21};
    FlowTestMarkerContext fs2 = {&order, 22};
    FlowTestMarkerContext ff = {&order, 99};

    loka::dsl::FlowChain<int, int> chain = //
        loka::dsl::Flow()
        | loka::dsl::Step(1, FlowTestMul2Adapter())
              .input(&input)
              .onSuccess(&captured)
              .onSuccess(&FlowTestMarker::onStepSuccess, &s1)
              .onFinally(&FlowTestMarker::onStepFinally, &f1)
        | loka::dsl::Step(2, FlowTestAdd1Adapter())
              .onSuccess(&FlowTestMarker::onStepSuccess, &s2)
              .onFinally(&FlowTestMarker::onStepFinally, &f2);
    chain.onSuccess(&FlowTestMarker::onFlowSuccess, &fs1);
    chain.onSuccess(&FlowTestMarker::onFlowSuccess, &fs2);
    chain.onFinally(&FlowTestMarker::onStepFinally, &ff);

    const bool ok = chain.run();
    assert(ok);
    assert(captured == 6);
    assert(order.size() == 7);
    assert(order[0] == 10);
    assert(order[1] == 11);
    assert(order[2] == 20);
    assert(order[3] == 21);
    assert(order[4] == 12);
    assert(order[5] == 22);
    assert(order[6] == 99);
  }

  {
    int input = 7;
    std::vector<int> order;

    FlowTestMarkerContext s1 = {&order, 110};
    FlowTestMarkerContext f1 = {&order, 111};
    FlowTestMarkerContext fs = {&order, 112};
    FlowTestMarkerContext failMatchUnhandled = {&order, 130};
    FlowTestMarkerContext failDefaultHandled = {&order, 131};
    FlowTestMarkerContext f2 = {&order, 132};
    FlowTestMarkerContext s3 = {&order, 140};
    FlowTestMarkerContext flowFail = {&order, 150};
    FlowTestMarkerContext flowFinal = {&order, 199};

    loka::dsl::FlowChain<int, int> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, FlowTestMul2Adapter())
              .input(&input)
              .onSuccess(&FlowTestMarker::onStepSuccess, &s1)
              .onFinally(&FlowTestMarker::onStepFinally, &f1)
        | loka::dsl::Step(2, FlowTestFail500Adapter())
              .onFailure(&FlowTestMarker::is500, &FlowTestMarker::onStepFailureUnhandled, &failMatchUnhandled)
              .onFailure(&FlowTestMarker::onStepFailureHandled, &failDefaultHandled)
              .onFinally(&FlowTestMarker::onStepFinally, &f2)
        | loka::dsl::Step(3, FlowTestAdd1Adapter()).onSuccess(&FlowTestMarker::onStepSuccess, &s3);
    chain.onSuccess(&FlowTestMarker::onFlowSuccess, &fs);
    chain.onFailure(&FlowTestMarker::onFlowFailureHandled, &flowFail);
    chain.onFinally(&FlowTestMarker::onStepFinally, &flowFinal);

    const bool ok = chain.run();
    assert(ok);
    assert(order.size() == 6);
    assert(order[0] == 110);
    assert(order[1] == 111);
    assert(order[2] == 130);
    assert(order[3] == 132);
    assert(order[4] == 150);
    assert(order[5] == 199);

    // Flow failure handler must run, default step failure handler must not run
    // (first-match-wins).
    bool hasFlowFailure = false;
    bool hasDefaultStepFailure = false;
    bool hasStep3Success = false;
    for (std::size_t i = 0; i < order.size(); ++i)
    {
      if (order[i] == 150)
        hasFlowFailure = true;
      if (order[i] == 131)
        hasDefaultStepFailure = true;
      if (order[i] == 140)
        hasStep3Success = true;
    }
    assert(hasFlowFailure);
    assert(!hasDefaultStepFailure);
    assert(!hasStep3Success);
  }

  {
    int inputA = 5;
    int inputB = 41;
    int captured = 0;
    std::vector<int> order;

    FlowTestMarkerContext s1 = {&order, 210};
    FlowTestMarkerContext f1 = {&order, 211};
    FlowTestMarkerContext s2 = {&order, 220};
    FlowTestMarkerContext f2 = {&order, 221};
    FlowTestMarkerContext flowFinal = {&order, 299};

    loka::dsl::FlowChain<int, int> chain = //
        loka::dsl::Flow()
        | loka::dsl::Step(1, FlowTestMul2Adapter())
              .input(&inputA)
              .onSuccess(&FlowTestMarker::onStepSuccess, &s1)
              .onFinally(&FlowTestMarker::onStepFinally, &f1)
        | loka::dsl::Step(2, FlowTestAdd1Adapter())
              .input(&inputB)
              .onSuccess(&captured)
              .onSuccess(&FlowTestMarker::onStepSuccess, &s2)
              .onFinally(&FlowTestMarker::onStepFinally, &f2);
    chain.onFinally(&FlowTestMarker::onStepFinally, &flowFinal);

    const bool resumedOk = chain.resume(2);
    assert(resumedOk);
    assert(captured == 42);
    assert(order.size() == 3);
    assert(order[0] == 220);
    assert(order[1] == 221);
    assert(order[2] == 299);

    const bool missing = chain.resume(9999);
    assert(!missing);
  }

  {
    int input = 10;
    bool loading = false;
    std::vector<int> order;
    FlowTestMarkerContext flowFinal = {&order, 399};

    // Keep this chain multi-line for readability of the DSL flow.
    loka::dsl::FlowChain<int, int> chain =
        loka::dsl::Flow() | loka::dsl::Step(1, FlowTestCheckLoadingAdapter(&loading)).input(&input);
    chain.trackLoading(&loading);
    chain.onFinally(&FlowTestMarker::onStepFinally, &flowFinal);

    assert(loading == false);
    const bool ok = chain.run();
    assert(ok);
    assert(loading == false);
    assert(order.size() == 1);
    assert(order[0] == 399);
  }

  {
    int input = 9;
    bool loading = false;
    std::vector<int> order;
    FlowTestMarkerContext flowFinal = {&order, 499};

    loka::dsl::FlowChain<int, int> chain = //
        loka::dsl::Flow()                  //
        | loka::dsl::Step(1, FlowTestFail500Adapter()).input(&input);
    chain.trackLoading(&loading);
    chain.onFinally(&FlowTestMarker::onStepFinally, &flowFinal);

    assert(loading == false);
    const bool ok = chain.run();
    assert(!ok);
    assert(loading == false);
    assert(order.size() == 1);
    assert(order[0] == 499);
  }

  {
    int input = 23;
    int calls = 0;
    bool ready = false;
    bool loading = false;
    int captured = 0;
    std::vector<int> order;
    FlowTestMarkerContext stepFinal = {&order, 510};
    FlowTestMarkerContext flowFinal = {&order, 599};

    loka::dsl::FlowChain<int, int> chain = loka::dsl::Flow()
                                           | loka::dsl::Step(1, FlowTestPendingThenSuccessAdapter(&ready, &calls))
                                                 .input(&input)
                                                 .onSuccess(&captured)
                                                 .onFinally(&FlowTestMarker::onStepFinally, &stepFinal);
    chain.trackLoading(&loading);
    chain.onFinally(&FlowTestMarker::onStepFinally, &flowFinal);

    const loka::dsl::FlowRunResult first = chain.runResult();
    assert(first == loka::dsl::FLOW_RUN_PENDING);
    assert(calls == 1);
    assert(loading == true);
    assert(order.empty()); // no finally on pending

    ready = true;
    const loka::dsl::FlowRunResult resumed = chain.resumeResult(1);
    assert(resumed == loka::dsl::FLOW_RUN_SUCCEEDED);
    assert(calls == 2);
    assert(captured == 123);
    assert(loading == false);
    assert(order.size() == 2);
    assert(order[0] == 510);
    assert(order[1] == 599);
  }

  {
    int input = 31;
    int calls = 0;
    bool ready = false;
    int captured = 0;
    std::vector<int> order;
    FlowTestMarkerContext flowFinal = {&order, 605};

    loka::dsl::FlowChain<int, int> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, FlowTestPendingThenSuccessAdapter(&ready, &calls)).input(&input).onSuccess(&captured);
    chain.onFinally(&FlowTestMarker::onStepFinally, &flowFinal);

    const loka::dsl::FlowRunResult first = chain.runResult();
    assert(first == loka::dsl::FLOW_RUN_PENDING);
    assert(calls == 1);
    assert(captured == 0);
    assert(order.empty());

    chain.cancel();
    ready = true;
    const loka::dsl::FlowRunResult canceled = chain.resumeResult(1);
    assert(canceled == loka::dsl::FLOW_RUN_CANCELED);
    assert(calls == 1);
    assert(captured == 0);
    assert(order.size() == 1);
    assert(order[0] == 605);
  }

  {
    int input = 37;
    int calls = 0;
    bool ready = false;
    FlowErrorCapture capture = {0, 0, 0};
    std::vector<int> order;
    FlowTestMarkerContext flowFinal = {&order, 607};

    loka::dsl::FlowChain<int, int> chain = loka::dsl::Flow()
                                           | loka::dsl::Step(1, FlowTestPendingThenSuccessAdapter(&ready, &calls))
                                                 .input(&input)
                                                 .timeoutPending(1)
                                                 .onFailure(&FlowTestMarker::captureFailure, &capture);
    chain.onFinally(&FlowTestMarker::onStepFinally, &flowFinal);

    assert(chain.runResult() == loka::dsl::FLOW_RUN_PENDING);
    assert(calls == 1);
    assert(capture.calls == 0);

    assert(chain.resumeResult(1) == loka::dsl::FLOW_RUN_SUCCEEDED);
    assert(calls == 2);
    assert(capture.calls == 1);
    assert(capture.kind == loka::dsl::FLOW_ERROR_KIND_FLOW);
    assert(capture.code == loka::dsl::FLOW_ERROR_CODE_STEP_PENDING_TIMEOUT);
    assert(order.size() == 1);
    assert(order[0] == 607);
  }

  {
    int input = 41;
    int calls = 0;
    bool ready = false;
    int captured = 0;
    std::vector<int> order;
    FlowTestMarkerContext flowFinal = {&order, 615};

    loka::dsl::FlowChain<int, int> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, FlowTestPendingThenSuccessAdapter(&ready, &calls)).input(&input).onSuccess(&captured);
    chain.onFinally(&FlowTestMarker::onStepFinally, &flowFinal);

    assert(chain.runResult() == loka::dsl::FLOW_RUN_PENDING);
    assert(calls == 1);

    chain.cancel();
    chain.clearCancel();
    ready = true;

    const loka::dsl::FlowRunResult resumed = chain.resumeResult(1);
    assert(resumed == loka::dsl::FLOW_RUN_SUCCEEDED);
    assert(calls == 2);
    assert(captured == 141);
    assert(order.size() == 1);
    assert(order[0] == 615);
  }

  {
    int input = 11;
    int out = 0;
    std::vector<int> order;
    FlowTestMarkerContext flowFinal = {&order, 625};

    loka::dsl::FlowChain<int, int> chain =
        loka::dsl::Flow() | loka::dsl::Step(1, FlowTestAdd1Adapter()).input(&input).onSuccess(&out);
    chain.onFinally(&FlowTestMarker::onStepFinally, &flowFinal);
    chain.cancel();

    const loka::dsl::FlowRunResult canceled = chain.runResult();
    assert(canceled == loka::dsl::FLOW_RUN_CANCELED);
    assert(out == 0);
    assert(order.size() == 1);
    assert(order[0] == 625);
  }

  {
    int input = 2;
    int out = 0;
    bool failedOnce = false;
    std::vector<int> order;
    FlowTestMarkerContext stepFail = {&order, 610};
    FlowTestMarkerContext stepSuccess = {&order, 611};
    FlowTestMarkerContext flowFinal = {&order, 699};

    loka::dsl::FlowChain<int, int> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, FlowTestFailOnceAdapter(&failedOnce))
              .input(&input)
              .onFailure(&FlowTestMarker::is500, &FlowTestMarker::onStepFailureHandled, &stepFail, 1)
              .onSuccess(&out)
              .onSuccess(&FlowTestMarker::onStepSuccess, &stepSuccess);
    chain.onFinally(&FlowTestMarker::onStepFinally, &flowFinal);

    const bool ok = chain.run();
    assert(ok);
    assert(out == 9);
    assert(order.size() == 3);
    assert(order[0] == 610);
    assert(order[1] == 611);
    assert(order[2] == 699);
  }

  {
    int input = 3;
    int out = 0;
    bool failedOnce = false;
    std::vector<int> order;
    FlowTestMarkerContext flowFail = {&order, 710};
    FlowTestMarkerContext flowSuccess = {&order, 711};
    FlowTestMarkerContext flowFinal = {&order, 799};

    loka::dsl::FlowChain<int, int> chain =
        loka::dsl::Flow() | loka::dsl::Step(1, FlowTestFailOnceAdapter(&failedOnce)).input(&input).onSuccess(&out);
    chain.onFailure(&FlowTestMarker::onFlowFailureHandled, &flowFail, 1);
    chain.onSuccess(&FlowTestMarker::onFlowSuccess, &flowSuccess);
    chain.onFinally(&FlowTestMarker::onStepFinally, &flowFinal);

    const bool ok = chain.run();
    assert(ok);
    assert(out == 10);
    assert(order.size() == 3);
    assert(order[0] == 710);
    assert(order[1] == 711);
    assert(order[2] == 799);
  }

  {
    int input = 7;
    int output = 0;

    loka::dsl::FlowChain<int, int> chain =
        loka::dsl::Flow() | loka::dsl::Step(1, FlowTestMul2Adapter()).input(&input)
        | loka::dsl::Step(2, loka::dsl::AssertPredicate<int>(&FlowTestPredicates::greaterThan10, 0))
        | loka::dsl::Step(3, FlowTestAdd1Adapter()).onSuccess(&output);

    assert(chain.run());
    assert(output == 15);
  }

  {
    int input = 4;
    int expected = 9;
    FlowErrorCapture capture = {0, 0, 0};

    loka::dsl::FlowChain<int, int> chain =
        loka::dsl::Flow() | loka::dsl::Step(1, FlowTestMul2Adapter()).input(&input)
        | loka::dsl::Step(2, loka::dsl::AssertPredicate<int>(&FlowTestPredicates::equalsExpected, &expected))
              .onFailure(&FlowTestMarker::captureFailure, &capture);

    assert(chain.run());
    assert(capture.calls == 1);
    assert(capture.kind == loka::dsl::FLOW_ERROR_KIND_FLOW);
    assert(capture.code == loka::dsl::FLOW_ERROR_CODE_ASSERT_PREDICATE_FAILED);
  }

  {
    int input = 10;
    int out = 0;
    int callsA = 0;
    int callsB = 0;
    std::vector<int> order;
    FlowTestMarkerContext flowFinal = {&order, 899};

    loka::dsl::FlowChain<int, int> chain =
        loka::dsl::Flow() | loka::dsl::Step(1, FlowTestCountedPassAdapter(&callsA)).input(&input).onSuccess(&out, 2)
        | loka::dsl::Step(2, FlowTestCountedPassAdapter(&callsB)).onSuccess(&out);
    chain.onFinally(&FlowTestMarker::onStepFinally, &flowFinal);

    const bool ok = chain.run();
    assert(ok);
    assert(callsA == 1);
    assert(callsB == 1);
    assert(out == 12);
    assert(order.size() == 1);
    assert(order[0] == 899);
  }

  {
    int input = 20;
    int out = 0;
    std::vector<int> flowMarks;
    FlowTestMarkerContext flowSuccess = {&flowMarks, 1};
    std::vector<int> order;
    FlowTestMarkerContext flowFinal = {&order, 999};

    loka::dsl::FlowChain<int, int> chain =
        loka::dsl::Flow() | loka::dsl::Step(1, FlowTestAdd1Adapter()).input(&input).onSuccess(&out);
    chain.onSuccess(&FlowTestMarker::onFlowSuccess, &flowSuccess, 9999);
    chain.onFinally(&FlowTestMarker::onStepFinally, &flowFinal);

    const bool ok = chain.run();
    assert(!ok);
    assert(flowMarks.size() == 1);
    assert(out == 21);
    assert(order.size() == 1);
    assert(order[0] == 999);
  }

  {
    int input = 4;
    std::vector<int> order;
    FlowTestMarkerContext step1Final = {&order, 1001};
    FlowTestMarkerContext step2Final = {&order, 1002};
    FlowTestMarkerContext flowFinal = {&order, 1099};

    loka::dsl::FlowChain<int, int> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, FlowTestMul2Adapter()).input(&input).onFinally(&FlowTestMarker::onStepFinally, &step1Final)
        | loka::dsl::Step(2, FlowTestAdd1Adapter()).onFinally(&FlowTestMarker::onStepFinally, &step2Final);
    chain.onFinally(&FlowTestMarker::onStepFinally, &flowFinal);

    assert(chain.run());
    assert(order.size() == 3);
    assert(order[0] == 1001);
    assert(order[1] == 1002);
    assert(order[2] == 1099);
  }

  {
    int input = 5;
    std::vector<int> order;
    FlowTestMarkerContext step1Final = {&order, 1101};
    FlowTestMarkerContext step2Final = {&order, 1102};
    FlowTestMarkerContext step3Final = {&order, 1103};
    FlowTestMarkerContext flowFinal = {&order, 1199};

    loka::dsl::FlowChain<int, int> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, FlowTestMul2Adapter()).input(&input).onFinally(&FlowTestMarker::onStepFinally, &step1Final)
        | loka::dsl::Step(2, FlowTestFail500Adapter()).onFinally(&FlowTestMarker::onStepFinally, &step2Final)
        | loka::dsl::Step(3, FlowTestAdd1Adapter()).onFinally(&FlowTestMarker::onStepFinally, &step3Final);
    chain.onFinally(&FlowTestMarker::onStepFinally, &flowFinal);

    assert(!chain.run());
    assert(order.size() == 3);
    assert(order[0] == 1101);
    assert(order[1] == 1102);
    assert(order[2] == 1199);
  }

  {
    int input = 6;
    std::vector<int> order;
    FlowTestMarkerContext stepFirstMatch = {&order, 1201};
    FlowTestMarkerContext stepDefault = {&order, 1202};
    FlowTestMarkerContext flowFinal = {&order, 1299};

    loka::dsl::FlowChain<int, int> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, FlowTestFail500Adapter())
              .input(&input)
              .onFailure(&FlowTestMarker::is500, &FlowTestMarker::onStepFailureUnhandled, &stepFirstMatch)
              .onFailure(&FlowTestMarker::onStepFailureHandled, &stepDefault);
    chain.onFinally(&FlowTestMarker::onStepFinally, &flowFinal);

    assert(!chain.run());
    assert(order.size() == 2);
    assert(order[0] == 1201);
    assert(order[1] == 1299);
  }

  {
    int input = 8;
    std::vector<int> order;
    FlowTestMarkerContext flowFirstMatch = {&order, 1301};
    FlowTestMarkerContext flowDefault = {&order, 1302};
    FlowTestMarkerContext flowFinal = {&order, 1399};

    loka::dsl::FlowChain<int, int> chain =
        loka::dsl::Flow() | loka::dsl::Step(1, FlowTestFail500Adapter()).input(&input);
    chain.onFailure(&FlowTestMarker::is500, &FlowTestMarker::onStepFailureUnhandled, &flowFirstMatch);
    chain.onFailure(&FlowTestMarker::onFlowFailureHandled, &flowDefault);
    chain.onFinally(&FlowTestMarker::onStepFinally, &flowFinal);

    assert(!chain.run());
    assert(order.size() == 2);
    assert(order[0] == 1301);
    assert(order[1] == 1399);
  }

  {
    int input = 9;
    std::vector<int> order;
    FlowTestMarkerContext flowNoMatch = {&order, 1401};
    FlowTestMarkerContext flowDefault = {&order, 1402};
    FlowTestMarkerContext flowFinal = {&order, 1499};

    loka::dsl::FlowChain<int, int> chain =
        loka::dsl::Flow() | loka::dsl::Step(1, FlowTestFail500Adapter()).input(&input);
    chain.onFailure(&FlowTestMarker::onFlowFailureHandled, &flowDefault);
    chain.onFailure(&FlowTestMarker::is500, &FlowTestMarker::onStepFailureUnhandled, &flowNoMatch);
    chain.onFinally(&FlowTestMarker::onStepFinally, &flowFinal);

    assert(chain.run());
    assert(order.size() == 2);
    assert(order[0] == 1402);
    assert(order[1] == 1499);
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;

    NodeComposition composition;
    BoxDefinition &root = composition.declare(Box().testId("RootBox"));
    root << Text("Hello Flow").testId("MainText");

    Scene scene(composition.root()->clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    Scene *scenePtr = &scene;
    loka::dsl::SnapRecord capture;

    loka::dsl::FlowChain<Scene *, loka::dsl::SnapRecord> chain =
        loka::dsl::Flow() | loka::dsl::Step(1, loka::dsl::testing::FindNodeById<TextNode>("MainText")).input(&scenePtr)
        | loka::dsl::Step(2, loka::dsl::testing::CaptureNode<TextNode>("SceneFlow", "capture-text", 1, 1))
              .onSuccess(&capture);

    assert(chain.run());
    std::string capturedText;
    assert(capture.get("node", capturedText));
    assert(capturedText == "MainText");
    assert(capture.get("text.value", capturedText));
    assert(capturedText == "Hello Flow");

    scene.unmount();
  }

  {
    using namespace loka::app::scene;

    loka::core::MutableState<int> countState(2);
    loka::core::PushStateTracker tracker;
    DummyStateOwner owner;
    loka::core::MutableState<loka::core::String> labelStorage;
    NodeState<loka::core::String> label(&labelStorage, &tracker, &owner);
    loka::dsl::StateStream<int> countStream(&countState, &tracker, &owner);
    loka::app::scene::FlowSlot<loka::dsl::StateStream<loka::core::String> > labelFlow;

    tracker.begin();
    labelFlow.set(countStream.map(loka::dsl::Const("Count: ") + countStream.slot.value())).bindTo(label, true);
    tracker.end();

    assert(label.isValid());
    assert(label.get().equals(loka::core::String::Literal("Count: 2")));

    tracker.begin();
    countState.set(5, true);
    tracker.end();

    assert(label.get().equals(loka::core::String::Literal("Count: 5")));
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;

    loka::core::MutableState<bool> showState(false);

    NodeComposition composition;
    BoxDefinition &root = composition.declare(Box().testId("RootBox"));
    TextDefinition trueText = Text("On").testId("ShowOnText");
    ShowDefinition showDef = Show(showState);
    showDef << trueText;
    assert(showDef.childrenCount() == 1);
    root << showDef;
    assert(root.childrenCount() == 1);
    assert(root.childrenHead() != 0);
    assert(root.childrenHead()->asNestableDefinition() == 0);

    Scene scene(composition.root()->clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    Scene *scenePtr = &scene;

    loka::dsl::FlowChain<Scene *, Scene *> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::testing::SetBoolStateAndFlush(&showState, true)).input(&scenePtr)
        | loka::dsl::Step(2, loka::dsl::testing::CheckText("ShowOnText", "On"));

    assert(chain.run());
    assert((platform.lastFlags_ & loka::app::scene::NODE_DIRTY_CHILD) != 0);
    assert((platform.lastFlags_ & loka::app::scene::NODE_DIRTY_LAYOUT) != 0);

    scene.unmount();
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;

    g_conditionalProjectedProbeLayoutCalls = 0;
    g_conditionalProjectedProbePrepareCalls = 0;

    loka::core::MutableState<bool> showState(false);

    NodeComposition composition;
    BoxDefinition &root = composition.declare(Box().testId("ConditionalProjectedRoot"));
    root << (Show(showState) << ConditionalProjectedProbeDefinition());

    Scene scene(composition.root()->clone());
    ConditionalProjectedProbeController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    assert(g_conditionalProjectedProbeLayoutCalls == 0);
    assert(g_conditionalProjectedProbePrepareCalls == 0);

    loka::app::scene::BoundaryNode *rootBoundary = loka::dsl::testing::SceneTestAccess::rootBoundary(scene);
    assert(rootBoundary);
    {
      loka::core::StateTrackerGuard guard(rootBoundary->tracker());
      showState.set(true);
    }
    scene.invalidate(loka::app::scene::NODE_DIRTY_CHILD);

    assert((platform.lastFlags_ & loka::app::scene::NODE_DIRTY_CHILD) != 0);
    assert(g_conditionalProjectedProbeLayoutCalls > 0);
    assert(g_conditionalProjectedProbePrepareCalls > 0);

    scene.unmount();
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;

    loka::core::MutableState<bool> showState(false);

    NodeComposition composition;
    BoxDefinition &root = composition.declare(Box().testId("RootBox"));
    root << (Show(showState) << Text("First").testId("ShowFirstText") << Text("Second").testId("ShowSecondText"));

    Scene scene(composition.root()->clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    Scene *scenePtr = &scene;

    loka::dsl::FlowChain<Scene *, Scene *> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::testing::SetBoolStateAndFlush(&showState, true)).input(&scenePtr)
        | loka::dsl::Step(2, loka::dsl::testing::CheckText("ShowFirstText", "First"))
        | loka::dsl::Step(3, loka::dsl::testing::CheckText("ShowSecondText", "Second"));

    assert(chain.run());
    assert((platform.lastFlags_ & loka::app::scene::NODE_DIRTY_CHILD) != 0);
    assert((platform.lastFlags_ & loka::app::scene::NODE_DIRTY_LAYOUT) != 0);

    scene.unmount();
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;

    NodeComposition composition;
    BoxDefinition &root = composition.declare(Box().testId("RootBox"));
    root << Text("Hello SnapText").testId("MainText");

    Scene scene(composition.root()->clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    Scene *scenePtr = &scene;
    loka::dsl::SnapRecord captured;

    loka::dsl::FlowChain<Scene *, loka::dsl::SnapRecord> okChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::testing::SnapText("MainText", "SceneFlow", "snap-text", 10, 1))
              .input(&scenePtr)
              .onSuccess(&captured);

    assert(okChain.run());
    std::string value;
    assert(captured.get("text.value", value));
    assert(value == "Hello SnapText");
    long dirtyMask = -1;
    assert(captured.getInt("dirty.mask", dirtyMask));
    assert(dirtyMask == 0);
    assert(captured.get("dirty.flags", value));
    assert(value == "NONE");

    FlowErrorCapture failCapture = {0, 0, 0};
    loka::dsl::FlowChain<Scene *, loka::dsl::SnapRecord> failChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::testing::SnapText("MissingText", "SceneFlow", "snap-text-missing", 11, 1))
              .input(&scenePtr)
              .onFailure(&FlowTestMarker::captureFailure, &failCapture);

    assert(failChain.run());
    assert(failCapture.calls == 1);
    assert(failCapture.kind == loka::dsl::testing::FLOW_ERROR_KIND_SCENE_SCENARIO);
    assert(failCapture.code == loka::dsl::testing::FLOW_ERROR_SCENE_TEST_NODE_NOT_FOUND);

    scene.unmount();
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;

    NodeComposition composition;
    BoxDefinition &root = composition.declare(Box().testId("RootBox"));
    root << Text("Hello Dirty").testId("MainText");

    Scene scene(composition.root()->clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    Scene *scenePtr = &scene;

    loka::dsl::FlowChain<Scene *, Scene *> okChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::testing::CheckTextDirtyEquals("MainText", loka::app::scene::NODE_DIRTY_NONE))
              .input(&scenePtr);

    assert(okChain.run());

    FlowErrorCapture failCapture = {0, 0, 0};
    loka::dsl::FlowChain<Scene *, Scene *> failChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::testing::CheckTextDirtyHasBits("MainText", loka::app::scene::NODE_DIRTY_LAYOUT))
              .input(&scenePtr)
              .onFailure(&FlowTestMarker::captureFailure, &failCapture);

    assert(failChain.run());
    assert(failCapture.calls == 1);
    assert(failCapture.kind == loka::dsl::testing::FLOW_ERROR_KIND_SCENE_TEST_ASSERT);
    assert(failCapture.code == loka::dsl::testing::FLOW_ERROR_SCENE_TEST_ASSERTION_FAILED);

    scene.unmount();
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;

    NodeComposition composition;
    BoxDefinition &root = composition.declare(Box().testId("RootBox"));
    root << Button("Press").testId("ActionButton");

    Scene scene(composition.root()->clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);
    loka::app::scene::Node *actionButton = 0;
    loka::dsl::FlowError lookupError;
    assert(loka::dsl::testing::LookupNodeById<loka::app::scene::Node>(
               &scene, std::string("ActionButton"), actionButton, lookupError)
           == loka::dsl::FLOW_STEP_SUCCEEDED);
    assert(actionButton);
    actionButton->setContext(new FlowTestCapturableNodeContext());

    Scene *scenePtr = &scene;
    loka::dsl::testing::ViewBitmapCapture bitmap;

    loka::dsl::FlowChain<Scene *, loka::dsl::testing::ViewBitmapCapture> chain =
        loka::dsl::Flow() | loka::dsl::Step(1, loka::dsl::testing::CaptureViewTarget("ActionButton")).input(&scenePtr)
        | loka::dsl::Step(2, loka::dsl::testing::CaptureViewBitmap(&FlowTestViewBitmapCaptureHelpers::capture, 0))
              .onSuccess(&bitmap);

    assert(chain.run());
    assert(bitmap.captured);
    assert(bitmap.image.isValid());
    assert(bitmap.width == 64);
    assert(bitmap.height == 32);

    scene.unmount();
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;

    NodeComposition composition;
    BoxDefinition &root = composition.declare(Box().testId("RootBox"));
    root << Button("Press").testId("ActionButton");

    Scene scene(composition.root()->clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);
    loka::app::scene::Node *actionButton = 0;
    loka::dsl::FlowError lookupError;
    assert(loka::dsl::testing::LookupNodeById<loka::app::scene::Node>(
               &scene, std::string("ActionButton"), actionButton, lookupError)
           == loka::dsl::FLOW_STEP_SUCCEEDED);
    assert(actionButton);
    actionButton->setContext(new FlowTestCapturableNodeContext());

    Scene *scenePtr = &scene;
    loka::dsl::testing::ViewBitmapCapture bitmap;

    loka::dsl::FlowChain<Scene *, loka::dsl::testing::ViewBitmapCapture> chain =
        loka::dsl::Flow() | loka::dsl::Step(1, loka::dsl::testing::CaptureViewTarget("ActionButton")).input(&scenePtr)
        | loka::dsl::Step(2, loka::dsl::testing::CaptureViewBitmapFromPlatform()).onSuccess(&bitmap);

    assert(chain.run());
    assert(bitmap.captured);
    assert(bitmap.image.isValid());
    assert(bitmap.width == 80);
    assert(bitmap.height == 40);

    scene.unmount();
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;

    NodeComposition composition;
    BoxDefinition &root = composition.declare(Box().testId("RootBox"));
    root << Button("Press").testId("ActionButton");

    Scene scene(composition.root()->clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    Scene *scenePtr = &scene;
    loka::dsl::SnapRecord captured;

    loka::dsl::FlowChain<Scene *, loka::dsl::SnapRecord> chain =
        loka::dsl::Flow() | loka::dsl::Step(1, loka::dsl::testing::CaptureViewTarget("ActionButton")).input(&scenePtr)
        | loka::dsl::Step(2, loka::dsl::testing::CaptureViewBitmap(&FlowTestViewBitmapCaptureHelpers::capture, 0))
        | loka::dsl::Step(3, loka::dsl::testing::CaptureViewBitmapSnap("SceneFlow", "capture-view-bitmap", 50, 1))
              .onSuccess(&captured)
        | loka::dsl::Step(4, loka::dsl::testing::AssertSnapIntEquals("view.bitmap.captured", 1))
        | loka::dsl::Step(5, loka::dsl::testing::AssertSnapIntEquals("view.bitmap.image.valid", 1))
        | loka::dsl::Step(6, loka::dsl::testing::AssertSnapIntEquals("view.bitmap.width", 64))
        | loka::dsl::Step(7, loka::dsl::testing::AssertSnapIntEquals("view.bitmap.height", 32));

    assert(chain.run());

    scene.unmount();
  }

  {
    using namespace loka::app::scene;

    NodeDefinition<helloworld::MainProps, helloworld::MainNode> mainDef;
    Scene scene(mainDef.clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    Scene *scenePtr = &scene;
    loka::dsl::FlowChain<Scene *, Scene *> addChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::testing::ClickButtonByIdAndFlush("HelloWorld.LeftPanel.AddButton"))
              .input(&scenePtr);
    assert(addChain.run());

    loka::dsl::FlowChain<Scene *, loka::dsl::SnapRecord> addSnapChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1,
                          loka::dsl::testing::SnapText(
                              "HelloWorld.LeftPanel.Message", "HelloWorldFlow", "message-after-add", 61, 1))
              .input(&scenePtr)
        | loka::dsl::Step(2, loka::dsl::testing::AssertSnapStringEquals("text.value", "Hello, Loka! +Loka"));
    assert(addSnapChain.run());

    loka::dsl::FlowChain<Scene *, Scene *> probeChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::testing::ClickButtonByIdAndFlush("HelloWorld.LeftPanel.ProbeButton"))
              .input(&scenePtr);
    assert(probeChain.run());

    loka::dsl::FlowChain<Scene *, loka::dsl::SnapRecord> probeSnapChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1,
                          loka::dsl::testing::SnapText(
                              "HelloWorld.LeftPanel.ActionSummary", "HelloWorldFlow", "summary-after-probe", 62, 1))
              .input(&scenePtr)
        | loka::dsl::Step(2,
                          loka::dsl::testing::AssertSnapStringEquals("text.value", "Button enabled: yes / clicks: 1"));
    assert(probeSnapChain.run());

    loka::dsl::FlowChain<Scene *, Scene *> disableChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::testing::ClickButtonByIdAndFlush("HelloWorld.LeftPanel.ToggleEnabledButton"))
              .input(&scenePtr);
    assert(disableChain.run());

    loka::dsl::FlowChain<Scene *, loka::dsl::SnapRecord> disableSnapChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1,
                          loka::dsl::testing::SnapText(
                              "HelloWorld.LeftPanel.ActionSummary", "HelloWorldFlow", "summary-after-disable", 63, 1))
              .input(&scenePtr)
        | loka::dsl::Step(2,
                          loka::dsl::testing::AssertSnapStringEquals("text.value", "Button enabled: no / clicks: 1"));
    assert(disableSnapChain.run());

    scene.unmount();
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;

    NodeComposition composition;
    BoxDefinition &root = composition.declare(Box().testId("RootBox"));
    root << Button("Press").testId("ActionButton");

    Scene scene(composition.root()->clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    Scene *scenePtr = &scene;
    loka::dsl::SnapRecord captured;

    loka::dsl::FlowChain<Scene *, loka::dsl::SnapRecord> chain =
        loka::dsl::Flow() | loka::dsl::Step(1, loka::dsl::testing::CaptureViewTarget("ActionButton")).input(&scenePtr)
        | loka::dsl::Step(2, loka::dsl::testing::CaptureView("SceneFlow", "capture-view", 48, 1)).onSuccess(&captured)
        | loka::dsl::Step(3, loka::dsl::testing::AssertSnapIntEquals("view.target.present", 1))
        | loka::dsl::Step(4, loka::dsl::testing::AssertSnapStringEquals("view.node.test_id", "ActionButton"));

    assert(chain.run());
    long contextPresent = 1;
    assert(captured.getInt("view.context.present", contextPresent));
    assert(contextPresent == 0);

    scene.unmount();
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;

    NodeComposition composition;
    BoxDefinition &root = composition.declare(Box().testId("RootBox"));
    root << Text("Ready").testId("StatusText");

    Scene scene(composition.root()->clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    Scene *scenePtr = &scene;
    loka::dsl::SnapRecord captured;

    loka::dsl::FlowChain<Scene *, loka::dsl::SnapRecord> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::testing::CaptureScene("SceneFlow", "capture-scene", 40, 1))
              .input(&scenePtr)
              .onSuccess(&captured)
        | loka::dsl::Step(2, loka::dsl::testing::AssertSnapIntEquals("scene.root.present", 1))
        | loka::dsl::Step(3, loka::dsl::testing::AssertSnapIntEquals("scene.root_boundary.present", 1))
        | loka::dsl::Step(4, loka::dsl::testing::AssertSnapIntGreaterEqual("scene.root.kind", 0));

    assert(chain.run());

    scene.unmount();
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;

    loka::core::MutableState<loka::core::String> textState(loka::core::String::Literal("Before"));

    NodeComposition composition;
    BoxDefinition &root = composition.declare(Box().testId("RootBox"));
    root << Text(&textState).testId("MainText");

    Scene scene(composition.root()->clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    Scene *scenePtr = &scene;

    loka::dsl::FlowChain<Scene *, Scene *> okChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::testing::CheckTextDirtyEquals("MainText", loka::app::scene::NODE_DIRTY_NONE))
              .input(&scenePtr)
        | loka::dsl::Step(2, loka::dsl::testing::SetStringStateAndFlush(&textState, "After"))
        | loka::dsl::Step(3, loka::dsl::testing::CheckText("MainText", "After"));

    assert(okChain.run());
    assert((platform.lastFlags_ & loka::app::scene::NODE_DIRTY_PROPS) != 0);

    scene.unmount();
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;

    loka::core::MutableState<loka::core::String> textState(loka::core::String::Literal("Before"));

    NodeComposition composition;
    BoxDefinition &root = composition.declare(Box().testId("RootBox"));
    root << Text(&textState).attr(TextAttr().wrap(TEXT_WRAP_WORD)).testId("WrapText");

    Scene scene(composition.root()->clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    Scene *scenePtr = &scene;

    loka::dsl::FlowChain<Scene *, Scene *> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::testing::SetStringStateAndFlush(&textState, "A longer wrapped line"))
              .input(&scenePtr)
        | loka::dsl::Step(2, loka::dsl::testing::CheckText("WrapText", "A longer wrapped line"));

    assert(chain.run());
    assert((platform.lastFlags_ & loka::app::scene::NODE_DIRTY_PROPS) != 0);
    assert((platform.lastFlags_ & loka::app::scene::NODE_DIRTY_LAYOUT) != 0);

    scene.unmount();
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;

    loka::core::MutableState<loka::core::String> textState(loka::core::String::Literal("Before"));
    loka::core::MutableState<bool> enabledState(false);

    NodeComposition composition;
    BoxDefinition &root = composition.declare(Box().testId("RootBox"));
    root << Text(&textState).attr(TextAttr().wrap(TEXT_WRAP_WORD)).testId("WrapText");
    root << Button("Run").enabled(&enabledState).testId("MainButton");

    Scene scene(composition.root()->clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    Scene *scenePtr = &scene;

    loka::dsl::FlowChain<Scene *, Scene *> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::testing::SetBoolStateAndFlush(&enabledState, true)).input(&scenePtr);

    assert(chain.run());
    assert((platform.lastFlags_ & loka::app::scene::NODE_DIRTY_PROPS) != 0);
    assert((platform.lastFlags_ & loka::app::scene::NODE_DIRTY_LAYOUT) == 0);

    scene.unmount();
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;

    loka::core::MutableState<bool> showState(false);
    loka::core::MutableState<bool> enabledState(false);

    NodeComposition composition;
    BoxDefinition &root = composition.declare(Box().testId("RootBox"));
    TextDefinition falseText = Text("Off").testId("OffText");
    TextDefinition trueText = Text("On").testId("OnText");
    root << composition.conditional(showState, trueText, falseText);
    root << Button("Run").enabled(&enabledState).testId("MainButton");

    Scene scene(composition.root()->clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    Scene *scenePtr = &scene;

    loka::dsl::FlowChain<Scene *, Scene *> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::testing::SetBoolStateAndFlush(&enabledState, true)).input(&scenePtr);

    assert(chain.run());
    assert((platform.lastFlags_ & loka::app::scene::NODE_DIRTY_PROPS) != 0);
    assert((platform.lastFlags_ & loka::app::scene::NODE_DIRTY_CHILD) == 0);

    scene.unmount();
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;

    g_sameBoundaryConditionalProbe = 0;

    Scene scene(BoundaryDefinition<SameBoundaryConditionalProbeProps, SameBoundaryConditionalProbeNode>().clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    Scene *scenePtr = &scene;

    loka::dsl::FlowChain<Scene *, Scene *> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::testing::CheckText("SameBoundaryOffText", "Off")).input(&scenePtr);

    assert(chain.run());
    assert(g_sameBoundaryConditionalProbe != 0);

    g_sameBoundaryConditionalProbe->emitToggle();

    loka::dsl::FlowChain<Scene *, Scene *> postToggleChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::testing::CheckText("SameBoundaryOnText", "On")).input(&scenePtr);

    assert(postToggleChain.run());
    assert((platform.lastFlags_ & loka::app::scene::NODE_DIRTY_CHILD) != 0);

    scene.unmount();
    g_sameBoundaryConditionalProbe = 0;
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;
    using namespace loka::dsl::testing;

    g_headlessScopeProbe = 0;
    g_headlessScopeHost = 0;
    g_headlessScopeAttachCount = 0;
    g_headlessScopeDetachCount = 0;

    Scene scene(BoundaryDefinition<HeadlessScopeHostProps, HeadlessScopeHostBoundaryNode>().clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    Scene *scenePtr = &scene;

    loka::dsl::FlowChain<Scene *, Scene *> initialChain =
        loka::dsl::Flow() | loka::dsl::Step(1, CheckText("HeadlessSummaryText", "Headless 0")).input(&scenePtr);

    assert(initialChain.run());
    assert(g_headlessScopeProbe != 0);
    assert(g_headlessScopeHost != 0);
    assert(g_headlessScopeAttachCount == 1);
    assert(g_headlessScopeDetachCount == 0);
    HeadlessScopeProbeBoundaryNode *firstProbe = g_headlessScopeProbe;

    g_headlessScopeProbe->increment();

    loka::dsl::FlowChain<Scene *, Scene *> incrementedChain =
        loka::dsl::Flow() | loka::dsl::Step(1, CheckText("HeadlessSummaryText", "Headless 1")).input(&scenePtr);

    assert(incrementedChain.run());

    g_headlessScopeHost->setShown(false);
    (void)SceneTestAccess::flushInvalidation(scene);

    loka::dsl::FlowError hiddenLookupError;
    hiddenLookupError.kind = 0;
    hiddenLookupError.code = 0;
    loka::app::TextNode *hiddenNode = 0;
    assert(LookupNodeById<loka::app::TextNode>(&scene, "HeadlessSummaryText", hiddenNode, hiddenLookupError)
           == loka::dsl::FLOW_STEP_FAILED);
    assert(hiddenLookupError.kind == FLOW_ERROR_KIND_SCENE_SCENARIO);
    assert(hiddenLookupError.code == FLOW_ERROR_SCENE_TEST_NODE_NOT_FOUND);
    assert(g_headlessScopeAttachCount == 1);
    assert(g_headlessScopeDetachCount == 0);

    assert(g_headlessScopeProbe == firstProbe);

    scene.unmount();
    g_headlessScopeProbe = 0;
    g_headlessScopeHost = 0;
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;
    using namespace loka::dsl::testing;

    g_headlessOwnedProbe = 0;
    g_headlessOwnedHost = 0;
    g_headlessOwnedAttachCount = 0;
    g_headlessOwnedDestroyCount = 0;

    Scene scene(BoundaryDefinition<HeadlessOwnedHostProps, HeadlessOwnedHostBoundaryNode>().clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    assert(g_headlessOwnedProbe != 0);
    assert(g_headlessOwnedHost != 0);
    assert(g_headlessOwnedAttachCount == 1);
    assert(g_headlessOwnedDestroyCount == 0);
    assert(g_headlessOwnedProbe->summaryText().equals(loka::core::String::Literal("Owned 0")));

    g_headlessOwnedProbe->increment();
    assert(g_headlessOwnedProbe->summaryText().equals(loka::core::String::Literal("Owned 1")));

    HeadlessOwnedProbeNode *firstOwnedProbe = g_headlessOwnedProbe;
    g_headlessOwnedHost->setShown(false);
    (void)SceneTestAccess::flushInvalidation(scene);
    assert(g_headlessOwnedDestroyCount == 0);
    assert(g_headlessOwnedProbe == firstOwnedProbe);
    assert(firstOwnedProbe != 0);

    g_headlessOwnedHost->setShown(true);
    (void)SceneTestAccess::flushInvalidation(scene);
    assert(g_headlessOwnedProbe != 0);
    assert(g_headlessOwnedProbe == firstOwnedProbe);
    assert(g_headlessOwnedAttachCount == 1);
    assert(g_headlessOwnedDestroyCount == 0);
    assert(g_headlessOwnedProbe->summaryText().equals(loka::core::String::Literal("Owned 1")));

    scene.unmount();
    g_headlessOwnedProbe = 0;
    g_headlessOwnedHost = 0;
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;
    using loka::dsl::testing::SceneTestAccess;

    g_headlessOwnedProbe = 0;
    g_headlessOwnedHost = 0;
    g_headlessOwnedAttachCount = 0;
    g_headlessOwnedDestroyCount = 0;

    Scene scene(BoundaryDefinition<HeadlessOwnedHostProps, HeadlessOwnedHostBoundaryNode>().clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    HeadlessOwnedProbeNode *firstOwnedProbe = g_headlessOwnedProbe;
    assert(firstOwnedProbe != 0);
    assert(g_headlessOwnedAttachCount == 1);
    assert(g_headlessOwnedDestroyCount == 0);

    (void)SceneTestAccess::flushInvalidation(scene);

    assert(g_headlessOwnedProbe == firstOwnedProbe);
    assert(g_headlessOwnedAttachCount == 1);
    assert(g_headlessOwnedDestroyCount == 0);

    scene.unmount();
    g_headlessOwnedProbe = 0;
    g_headlessOwnedHost = 0;
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;
    using loka::dsl::testing::SceneTestAccess;

    g_headlessOwnedMultiProbeA = 0;
    g_headlessOwnedMultiProbeB = 0;
    g_headlessOwnedMultiHost = 0;
    g_headlessOwnedMultiAttachCountA = 0;
    g_headlessOwnedMultiAttachCountB = 0;
    g_headlessOwnedMultiDestroyCountA = 0;
    g_headlessOwnedMultiDestroyCountB = 0;

    Scene scene(BoundaryDefinition<HeadlessOwnedMultiHostProps, HeadlessOwnedMultiHostBoundaryNode>().clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    assert(g_headlessOwnedMultiProbeA != 0);
    assert(g_headlessOwnedMultiProbeB != 0);
    assert(g_headlessOwnedMultiHost != 0);
    assert(g_headlessOwnedMultiAttachCountA == 1);
    assert(g_headlessOwnedMultiAttachCountB == 1);
    assert(g_headlessOwnedMultiDestroyCountA == 0);
    assert(g_headlessOwnedMultiDestroyCountB == 0);
    assert(g_headlessOwnedMultiProbeA->summaryText().equals(loka::core::String::Literal("OwnedA 0")));
    assert(g_headlessOwnedMultiProbeB->summaryText().equals(loka::core::String::Literal("OwnedB 0")));

    HeadlessOwnedMultiProbeANode *firstProbeA = g_headlessOwnedMultiProbeA;
    HeadlessOwnedMultiProbeBNode *firstProbeB = g_headlessOwnedMultiProbeB;

    g_headlessOwnedMultiHost->setShown(false);
    (void)SceneTestAccess::flushInvalidation(scene);
    assert(g_headlessOwnedMultiProbeA == firstProbeA);
    assert(g_headlessOwnedMultiProbeB == firstProbeB);
    assert(g_headlessOwnedMultiDestroyCountA == 0);
    assert(g_headlessOwnedMultiDestroyCountB == 0);
    assert(firstProbeA != 0);
    assert(firstProbeB != 0);

    g_headlessOwnedMultiHost->setShown(true);
    (void)SceneTestAccess::flushInvalidation(scene);
    assert(g_headlessOwnedMultiProbeA != 0);
    assert(g_headlessOwnedMultiProbeB != 0);
    assert(g_headlessOwnedMultiProbeA == firstProbeA);
    assert(g_headlessOwnedMultiProbeB == firstProbeB);
    assert(g_headlessOwnedMultiAttachCountA == 1);
    assert(g_headlessOwnedMultiAttachCountB == 1);
    assert(g_headlessOwnedMultiDestroyCountA == 0);
    assert(g_headlessOwnedMultiDestroyCountB == 0);
    assert(g_headlessOwnedMultiProbeA->summaryText().equals(loka::core::String::Literal("OwnedA 0")));
    assert(g_headlessOwnedMultiProbeB->summaryText().equals(loka::core::String::Literal("OwnedB 0")));

    scene.unmount();
    g_headlessOwnedMultiProbeA = 0;
    g_headlessOwnedMultiProbeB = 0;
    g_headlessOwnedMultiHost = 0;
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;
    using loka::dsl::testing::SceneTestAccess;

    g_headlessOwnedProbe = 0;
    g_headlessOwnedAttachCount = 0;
    g_headlessOwnedDestroyCount = 0;
    g_headlessOwnedPersistentProbe = 0;
    g_headlessOwnedPersistentAttachCount = 0;
    g_headlessOwnedPersistentDestroyCount = 0;

    Scene scene(BoundaryDefinition<HeadlessOwnedMixedHostProps, HeadlessOwnedMixedHostBoundaryNode>().clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    assert(g_headlessOwnedPersistentProbe != 0);
    assert(g_headlessOwnedProbe != 0);
    assert(g_headlessOwnedPersistentAttachCount == 1);
    assert(g_headlessOwnedPersistentDestroyCount == 0);
    assert(g_headlessOwnedAttachCount == 1);
    assert(g_headlessOwnedDestroyCount == 0);

    HeadlessOwnedPersistentProbeNode *persistentProbe = g_headlessOwnedPersistentProbe;
    HeadlessOwnedProbeNode *ownedProbe = g_headlessOwnedProbe;

    static_cast<HeadlessOwnedMixedHostBoundaryNode *>(SceneTestAccess::rootBoundary(scene))->setShown(false);
    (void)SceneTestAccess::flushInvalidation(scene);
    assert(g_headlessOwnedPersistentProbe == persistentProbe);
    assert(g_headlessOwnedPersistentAttachCount == 1);
    assert(g_headlessOwnedPersistentDestroyCount == 0);
    assert(g_headlessOwnedProbe == ownedProbe);
    assert(g_headlessOwnedDestroyCount == 0);

    static_cast<HeadlessOwnedMixedHostBoundaryNode *>(SceneTestAccess::rootBoundary(scene))->setShown(true);
    (void)SceneTestAccess::flushInvalidation(scene);
    assert(g_headlessOwnedPersistentProbe == persistentProbe);
    assert(g_headlessOwnedPersistentAttachCount == 1);
    assert(g_headlessOwnedPersistentDestroyCount == 0);
    assert(g_headlessOwnedProbe == ownedProbe);
    assert(g_headlessOwnedAttachCount == 1);
    assert(g_headlessOwnedDestroyCount == 0);

    scene.unmount();
    g_headlessOwnedProbe = 0;
    g_headlessOwnedPersistentProbe = 0;
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;

    loka::core::MutableState<bool> enabledState(false);

    NodeComposition composition;
    BoxDefinition &root = composition.declare(Box().testId("RootBox"));
    root << Button("Run").enabled(&enabledState).testId("MainButton");

    Scene scene(composition.root()->clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    Scene *scenePtr = &scene;

    loka::dsl::FlowChain<Scene *, Scene *> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::testing::SetBoolStateAndFlush(&enabledState, true)).input(&scenePtr);

    assert(chain.run());
    assert((platform.lastFlags_ & loka::app::scene::NODE_DIRTY_PROPS) != 0);

    scene.unmount();
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;

    loka::core::MutableState<bool> enabledState(false);

    NodeComposition composition;
    BoxDefinition &root = composition.declare(Box().testId("RootBox"));
    root << Button("Run").enabled(&enabledState).testId("MainButton");

    Scene scene(composition.root()->clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    Scene *scenePtr = &scene;
    loka::dsl::SnapRecord captured;

    loka::dsl::FlowChain<Scene *, loka::dsl::SnapRecord> okChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::testing::SetBoolStateAndFlush(&enabledState, true)).input(&scenePtr)
        | loka::dsl::Step(2, FlowTestPlatformDirtyMaskAdapter("platform-props-dirty", &platform, 24))
              .onSuccess(&captured)
        | loka::dsl::Step(
            3, loka::dsl::testing::AssertSnapIntMaskHasBits("platform.dirty.mask", loka::app::scene::NODE_DIRTY_PROPS))
        | loka::dsl::Step(4, loka::dsl::testing::AssertSnapIntEquals("platform.materialized", 1));

    assert(okChain.run());
    long dirtyMask = 0;
    assert(captured.getInt("platform.dirty.mask", dirtyMask));
    assert((dirtyMask & loka::app::scene::NODE_DIRTY_PROPS) != 0);
    assert((dirtyMask & loka::app::scene::NODE_DIRTY_LAYOUT) == 0);
    assert((dirtyMask & loka::app::scene::NODE_DIRTY_CHILD) == 0);
    long fullRebuild = 1;
    assert(captured.getInt("platform.full_rebuild", fullRebuild));
    assert(fullRebuild == 0);
    long platformCalls = 0;
    assert(captured.getInt("platform.calls", platformCalls));
    assert(platformCalls >= 1);

    scene.unmount();
  }

  {
    using namespace loka::app::scene;
    using loka::dsl::testing::BoundaryObservedStateTestAccess;

    loka::core::MutableState<int> observedValue(1);
    loka::core::PushStateTracker tracker;
    tracker.addState(&observedValue);

    BoundaryObservedState observedState;
    observedState.beginPass();
    BoundaryObservedStateTestAccess::appendObservedEntry(observedState, &observedValue, NODE_DIRTY_PROPS);

    {
      loka::core::StateTrackerGuard guard(&tracker);
      observedValue.set(2, true);
    }
    assert(observedState.dirtyFlagsForCommittedStates(&tracker) == NODE_DIRTY_PROPS);

    observedState.beginPass();
    {
      loka::core::StateTrackerGuard guard(&tracker);
      observedValue.set(3, true);
    }
    assert(observedState.dirtyFlagsForCommittedStates(&tracker) == NODE_DIRTY_NONE);

    BoundaryObservedStateTestAccess::updateFirstEntryForCurrentPass(observedState, NODE_DIRTY_LAYOUT);
    assert(observedState.dirtyFlagsForCommittedStates(&tracker) == NODE_DIRTY_LAYOUT);
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;
    using loka::dsl::testing::SceneTestAccess;

    Scene scene((BoundaryDefinition<PendingCompositedProbeBoundaryProps, PendingCompositedProbeBoundaryNode>()));
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    BoundaryNode *rootBoundary = SceneTestAccess::rootBoundary(scene);
    assert(rootBoundary != 0);

    scene.requestInvalidate(NODE_DIRTY_PROPS);
    assert(SceneTestAccess::snapshotEffectiveDirtyFlags(SceneTestAccess::updateSnapshot(scene)) == NODE_DIRTY_NONE);
    assert(SceneTestAccess::snapshotFirstPendingRoot(SceneTestAccess::updateSnapshot(scene)) == 0);
    assert(SceneTestAccess::snapshotRootBoundary(SceneTestAccess::updateSnapshot(scene)) == 0);
    assert(SceneTestAccess::snapshotGeneration(SceneTestAccess::updateSnapshot(scene)) == 0);
    assert(SceneTestAccess::snapshotEffectiveDirtyFlags(SceneTestAccess::lastUpdateSnapshot(scene)) == NODE_DIRTY_NONE);
    assert(SceneTestAccess::hasRequestedInput(scene));
    assert(SceneTestAccess::requestedDirtyFlags(scene) == NODE_DIRTY_PROPS);
    assert(SceneTestAccess::effectiveRequestedDirtyFlags(scene) == NODE_DIRTY_PROPS);
    assert(SceneTestAccess::requestedFullRebuild(scene) == false);
    assert(SceneTestAccess::projectionTransactionGeneration(scene) != 0);
    const unsigned long firstGeneration = SceneTestAccess::projectionTransactionGeneration(scene);
    assert(SceneTestAccess::director(scene).firstPendingBoundary() == rootBoundary);

    assert(scene.flushInvalidation());
    const SceneDirector::SceneUpdateSnapshot &observationAfterFlush = SceneTestAccess::updateSnapshot(scene);
    assert(SceneTestAccess::snapshotGeneration(observationAfterFlush) == 0);
    assert(SceneTestAccess::snapshotRequestedDirtyFlags(observationAfterFlush) == NODE_DIRTY_NONE);
    assert(SceneTestAccess::snapshotTransactionDirtyFlags(observationAfterFlush) == NODE_DIRTY_NONE);
    assert(SceneTestAccess::snapshotEffectiveDirtyFlags(observationAfterFlush) == NODE_DIRTY_NONE);
    assert(SceneTestAccess::snapshotRequestedFullRebuild(observationAfterFlush) == false);
    assert(SceneTestAccess::snapshotEffectiveFullRebuild(observationAfterFlush) == false);
    assert(SceneTestAccess::snapshotFirstPendingRoot(observationAfterFlush) == 0);
    assert(SceneTestAccess::snapshotRootBoundary(observationAfterFlush) == 0);
    assert(SceneTestAccess::snapshotPrimaryRoot(observationAfterFlush) == 0);
    assert(SceneTestAccess::snapshotLayoutRequired(observationAfterFlush) == false);
    assert(SceneTestAccess::snapshotStructureRequired(observationAfterFlush) == false);
    assert(SceneTestAccess::snapshotCompositedPaintRequired(observationAfterFlush) == false);
    assert(SceneTestAccess::snapshotOpaqueLocalPaintRequired(observationAfterFlush) == false);
    assert(SceneTestAccess::snapshotLocalCompositionDiffApplicable(observationAfterFlush) == false);
    const SceneDirector::SceneUpdateSnapshot &lastObservationAfterFlush = SceneTestAccess::lastUpdateSnapshot(scene);
    assert(SceneTestAccess::snapshotGeneration(lastObservationAfterFlush) == firstGeneration);
    assert((SceneTestAccess::snapshotRequestedDirtyFlags(lastObservationAfterFlush) & NODE_DIRTY_PROPS) != 0);
    assert((SceneTestAccess::snapshotTransactionDirtyFlags(lastObservationAfterFlush) & NODE_DIRTY_PROPS) != 0);
    assert((SceneTestAccess::snapshotEffectiveDirtyFlags(lastObservationAfterFlush) & NODE_DIRTY_PROPS) != 0);
    assert(SceneTestAccess::snapshotFirstPendingRoot(lastObservationAfterFlush) == rootBoundary);
    assert(SceneTestAccess::snapshotRootBoundary(lastObservationAfterFlush) == rootBoundary);
    assert(SceneTestAccess::snapshotPrimaryRoot(lastObservationAfterFlush) == rootBoundary);
    assert(SceneTestAccess::snapshotRequestedFullRebuild(lastObservationAfterFlush) == false);
    assert(SceneTestAccess::snapshotEffectiveFullRebuild(lastObservationAfterFlush) == false);
    assert(SceneTestAccess::hasRequestedInput(scene) == false);
    assert(SceneTestAccess::requestedDirtyFlags(scene) == NODE_DIRTY_NONE);
    assert(SceneTestAccess::effectiveRequestedDirtyFlags(scene) == NODE_DIRTY_PROPS);
    assert(SceneTestAccess::requestedFullRebuild(scene) == false);
    assert(SceneTestAccess::projectionTransactionGeneration(scene) == 0);
    assert(SceneTestAccess::director(scene).firstPendingBoundary() == 0);

    scene.requestInvalidate(static_cast<NodeDirtyFlags>(NODE_DIRTY_PROPS | NODE_DIRTY_LAYOUT));
    const unsigned long secondGeneration = SceneTestAccess::projectionTransactionGeneration(scene);
    assert(secondGeneration != 0);
    assert(secondGeneration != firstGeneration);
    assert(SceneTestAccess::hasRequestedInput(scene));
    assert((SceneTestAccess::requestedDirtyFlags(scene) & NODE_DIRTY_LAYOUT) != 0);
    assert((SceneTestAccess::effectiveRequestedDirtyFlags(scene) & NODE_DIRTY_LAYOUT) != 0);
    assert(SceneTestAccess::requestedFullRebuild(scene) == false);
    assert(SceneTestAccess::director(scene).firstPendingBoundary() == rootBoundary);
    assert(scene.flushInvalidation());
    const PlatformApplyPlan &plan = SceneTestAccess::lastApplyPlan(scene);
    assert(plan.layoutRoot == rootBoundary);
    assert(plan.paintRoot == rootBoundary);
    const SceneDirector::SceneUpdateSnapshot &lastLayoutObservation = SceneTestAccess::lastUpdateSnapshot(scene);
    assert(SceneTestAccess::snapshotGeneration(lastLayoutObservation) == secondGeneration);
    assert((SceneTestAccess::snapshotRequestedDirtyFlags(lastLayoutObservation) & NODE_DIRTY_LAYOUT) != 0);
    assert((SceneTestAccess::snapshotEffectiveDirtyFlags(lastLayoutObservation) & NODE_DIRTY_LAYOUT) != 0);
    assert(SceneTestAccess::snapshotRootBoundary(lastLayoutObservation) == rootBoundary);
    assert(SceneTestAccess::snapshotPrimaryRoot(lastLayoutObservation) == rootBoundary);
    assert(SceneTestAccess::hasRequestedInput(scene) == false);
    assert(SceneTestAccess::requestedDirtyFlags(scene) == NODE_DIRTY_NONE);
    assert(SceneTestAccess::effectiveRequestedDirtyFlags(scene) == NODE_DIRTY_PROPS);
    assert(SceneTestAccess::projectionTransactionGeneration(scene) == 0);
    assert(SceneTestAccess::director(scene).firstPendingBoundary() == 0);

    scene.unmount();
    const SceneDirector::SceneUpdateSnapshot &observationAfterUnmount = SceneTestAccess::updateSnapshot(scene);
    assert(SceneTestAccess::snapshotGeneration(observationAfterUnmount) == 0);
    assert(SceneTestAccess::snapshotEffectiveDirtyFlags(observationAfterUnmount) == NODE_DIRTY_NONE);
    assert(SceneTestAccess::snapshotFirstPendingRoot(observationAfterUnmount) == 0);
    assert(SceneTestAccess::snapshotRootBoundary(observationAfterUnmount) == 0);
    assert(SceneTestAccess::snapshotPrimaryRoot(observationAfterUnmount) == 0);
    const SceneDirector::SceneUpdateSnapshot &lastObservationAfterUnmount = SceneTestAccess::lastUpdateSnapshot(scene);
    assert(SceneTestAccess::snapshotGeneration(lastObservationAfterUnmount) == 0);
    assert(SceneTestAccess::snapshotEffectiveDirtyFlags(lastObservationAfterUnmount) == NODE_DIRTY_NONE);
    assert(SceneTestAccess::snapshotFirstPendingRoot(lastObservationAfterUnmount) == 0);
    assert(SceneTestAccess::snapshotRootBoundary(lastObservationAfterUnmount) == 0);
    assert(SceneTestAccess::snapshotPrimaryRoot(lastObservationAfterUnmount) == 0);
    assert(SceneTestAccess::lastApplyPlan(scene).hasAnyWork() == false);
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;
    using loka::dsl::testing::SceneTestAccess;

    Scene scene((BoundaryDefinition<PendingCompositedProbeBoundaryProps, PendingCompositedProbeBoundaryNode>()));
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    BoundaryNode *rootBoundary = SceneTestAccess::rootBoundary(scene);
    assert(rootBoundary != 0);

    scene.requestInvalidate(NODE_DIRTY_PROPS);
    assert(SceneTestAccess::projectionTransactionHasPending(scene));
    assert(SceneTestAccess::projectionTransactionTargetCount(scene) == 1);
    assert(SceneTestAccess::projectionTransactionAggregateDirtyFlags(scene) == NODE_DIRTY_PROPS);
    assert(SceneTestAccess::projectionTransactionGeneration(scene) != 0);
    assert(SceneTestAccess::projectionTransactionFirstTargetNode(scene) == rootBoundary);
    assert(SceneTestAccess::projectionTransactionFirstTargetDirtyFlags(scene) == NODE_DIRTY_PROPS);

    scene.requestInvalidate(NODE_DIRTY_LAYOUT);
    assert(SceneTestAccess::projectionTransactionHasPending(scene));
    assert(SceneTestAccess::projectionTransactionTargetCount(scene) == 1);
    assert((SceneTestAccess::projectionTransactionAggregateDirtyFlags(scene) & NODE_DIRTY_PROPS) != 0);
    assert((SceneTestAccess::projectionTransactionAggregateDirtyFlags(scene) & NODE_DIRTY_LAYOUT) != 0);
    assert(SceneTestAccess::projectionTransactionFirstTargetNode(scene) == rootBoundary);
    assert((SceneTestAccess::projectionTransactionFirstTargetDirtyFlags(scene) & NODE_DIRTY_PROPS) != 0);
    assert((SceneTestAccess::projectionTransactionFirstTargetDirtyFlags(scene) & NODE_DIRTY_LAYOUT) != 0);

    assert(scene.flushInvalidation());
    assert(!SceneTestAccess::projectionTransactionHasPending(scene));
    assert(SceneTestAccess::projectionTransactionTargetCount(scene) == 0);
    assert(SceneTestAccess::projectionTransactionAggregateDirtyFlags(scene) == NODE_DIRTY_NONE);
    assert(SceneTestAccess::projectionTransactionGeneration(scene) == 0);
    assert(SceneTestAccess::projectionTransactionFirstTargetNode(scene) == 0);

    scene.unmount();
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;
    using loka::dsl::testing::SceneTestAccess;

    Scene scene((BoundaryDefinition<PendingCompositedProbeBoundaryProps, PendingCompositedProbeBoundaryNode>()));
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    BoundaryNode *rootBoundary = SceneTestAccess::rootBoundary(scene);
    assert(rootBoundary != 0);
    scene.requestInvalidate(NODE_DIRTY_PROPS);
    assert(SceneTestAccess::director(scene).firstPendingBoundary() == rootBoundary);
    assert(scene.flushInvalidation());
    const PlatformApplyPlan &plan = SceneTestAccess::lastApplyPlan(scene);
    assert(plan.paintKind == PlatformApplyPlan::PAINT_COMPOSITED);
    assert(plan.paintRoot == rootBoundary);
    assert(SceneTestAccess::director(scene).firstPendingBoundary() == 0);

    scene.unmount();
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;
    using loka::dsl::testing::SceneTestAccess;

    g_defaultApplyLocalPaintCalls = 0;
    g_defaultApplyOpaquePaintCalls = 0;
    g_defaultApplyCompositedPaintCalls = 0;
    g_defaultApplyLayoutCalls = 0;
    g_defaultApplyStructureCalls = 0;
    g_defaultApplyBoundsWidth = 0;
    g_defaultApplyBoundsHeight = 0;
    g_defaultApplyOpaqueHintSeen = 0;

    Scene scene((BoundaryDefinition<PendingDefaultApplyProbeBoundaryProps, PendingDefaultApplyProbeBoundaryNode>()));
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    scene.requestInvalidate(static_cast<NodeDirtyFlags>(NODE_DIRTY_PROPS | NODE_DIRTY_LAYOUT));
    BoundaryNode *rootBoundary = SceneTestAccess::rootBoundary(scene);
    assert(rootBoundary != 0);
    assert(SceneTestAccess::director(scene).firstPendingBoundary() == rootBoundary);
    assert(scene.flushInvalidation());
    assert(g_defaultApplyLayoutCalls == 1);
    assert(g_defaultApplyCompositedPaintCalls == 1);
    assert(g_defaultApplyLocalPaintCalls == 0);
    assert(g_defaultApplyOpaquePaintCalls == 1);
    assert(g_defaultApplyOpaqueHintSeen == 1);
    assert(g_defaultApplyBoundsWidth == 40);
    assert(g_defaultApplyBoundsHeight == 12);
    assert(SceneTestAccess::director(scene).firstPendingBoundary() == 0);
    assert(SceneTestAccess::lastApplyPlan(scene).paintKind == PlatformApplyPlan::PAINT_COMPOSITED);
    assert(SceneTestAccess::lastApplyPlan(scene).structureRoot == rootBoundary);

    g_defaultApplyLocalPaintCalls = 0;
    g_defaultApplyOpaquePaintCalls = 0;
    g_defaultApplyCompositedPaintCalls = 0;
    g_defaultApplyLayoutCalls = 0;
    g_defaultApplyStructureCalls = 0;

    scene.requestInvalidate(NODE_DIRTY_CHILD);
    assert(SceneTestAccess::director(scene).firstPendingBoundary() == rootBoundary);
    assert(scene.flushInvalidation());
    assert(g_defaultApplyStructureCalls == 0);
    assert(g_defaultApplyLayoutCalls == 1);
    assert(g_defaultApplyCompositedPaintCalls == 1);
    assert(SceneTestAccess::lastApplyPlan(scene).structureChanged == false);

    scene.unmount();
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;
    using loka::dsl::testing::SceneTestAccess;

    Scene scene((BoundaryDefinition<PendingCompositedProbeBoundaryProps, PendingCompositedProbeBoundaryNode>()));
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    BoundaryNode *rootBoundary = SceneTestAccess::rootBoundary(scene);
    assert(rootBoundary != 0);
    const int baselineCalls = platform.calls_;

    scene.requestBoundaryUpdate(rootBoundary, NODE_DIRTY_NONE, true);

    assert(platform.calls_ > baselineCalls);
    assert((SceneTestAccess::lastApplyPlan(scene).paintKind == PlatformApplyPlan::PAINT_COMPOSITED)
           || (SceneTestAccess::lastApplyPlan(scene).paintKind == PlatformApplyPlan::PAINT_LOCAL_OPAQUE)
           || (SceneTestAccess::lastApplyPlan(scene).paintKind == PlatformApplyPlan::PAINT_LOCAL));
    assert((SceneTestAccess::snapshotRequestedDirtyFlags(SceneTestAccess::lastUpdateSnapshot(scene)) & NODE_DIRTY_PROPS)
           != 0);
    assert((SceneTestAccess::snapshotEffectiveDirtyFlags(SceneTestAccess::lastUpdateSnapshot(scene)) & NODE_DIRTY_PROPS)
           != 0);
    assert(SceneTestAccess::director(scene).firstPendingBoundary() == 0);
    assert(SceneTestAccess::projectionTransactionGeneration(scene) == 0);

    scene.unmount();
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;
    using loka::dsl::testing::SceneTestAccess;

    g_pendingLayoutWidthState.set(32);

    Scene scene((BoundaryDefinition<PendingLayoutBoundaryProps, PendingLayoutBoundaryNode>()));
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    Scene *scenePtr = &scene;
    loka::dsl::SnapRecord captured;
    FlowErrorCapture failCapture = {0, 0, 0};

    loka::dsl::FlowChain<Scene *, loka::dsl::SnapRecord> okChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::testing::SetIntStateAndFlush(&g_pendingLayoutWidthState, 64)).input(&scenePtr)
        | loka::dsl::Step(2, loka::dsl::testing::CheckText("PendingLayoutText", "Sized"))
        | loka::dsl::Step(3, FlowTestPlatformDirtyMaskAdapter("pending-layout-upgrade", &platform, 20))
              .onSuccess(&captured)
        | loka::dsl::Step(
            4, loka::dsl::testing::AssertSnapIntMaskHasBits("platform.dirty.mask", loka::app::scene::NODE_DIRTY_PROPS))
        | loka::dsl::Step(
            5,
            loka::dsl::testing::AssertSnapIntMaskHasBits("platform.dirty.mask", loka::app::scene::NODE_DIRTY_LAYOUT));

    okChain.onFailure(&FlowTestMarker::captureFailure, &failCapture);
    const bool ok = okChain.run();
    if (!ok)
    {
      std::printf("[pending-layout-upgrade] fail kind=%d code=%d calls=%d full=%d flags=%ld\n",
                  failCapture.kind,
                  failCapture.code,
                  failCapture.calls,
                  platform.lastFullRebuild_ ? 1 : 0,
                  static_cast<long>(platform.lastFlags_));
      std::fflush(stdout);
    }
    assert(ok);
    assert(SceneTestAccess::director(scene).firstPendingBoundary() == 0);

    scene.unmount();
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;
    using loka::dsl::testing::SceneTestAccess;

    g_pendingApplyCallCount = 0;
    g_pendingApplyWhileApplyingCount = 0;

    Scene scene((BoundaryDefinition<PendingApplyProbeBoundaryProps, PendingApplyProbeBoundaryNode>()));
    FlowScenePlatformController platform;
    platform.skipGlobalChangeForBoundaryLocalPaint_ = true;
    scene.mount(&platform);
    scene.updateAttached(true);
    platform.calls_ = 0;
    platform.boundaryApplyCalls_ = 0;
    platform.lastFlags_ = loka::app::scene::NODE_DIRTY_NONE;

    scene.requestInvalidate(NODE_DIRTY_PROPS);
    BoundaryNode *rootBoundary = SceneTestAccess::rootBoundary(scene);
    assert(rootBoundary != 0);
    assert(SceneTestAccess::director(scene).firstPendingBoundary() == rootBoundary);
    assert(scene.flushInvalidation());
    assert(g_pendingApplyCallCount == 1);
    assert(g_pendingApplyWhileApplyingCount == 1);
    assert(platform.boundaryApplyCalls_ == 1);
    assert(platform.lastBoundaryApplyRoot_ == SceneTestAccess::rootNode(scene));
    assert(platform.lastBoundaryApplyBoundary_ == rootBoundary);
    const PlatformApplyPlan &plan = SceneTestAccess::lastApplyPlan(scene);
    assert(plan.paintKind == PlatformApplyPlan::PAINT_LOCAL);
    assert(plan.layoutRoot == rootBoundary);
    assert(plan.paintRoot == rootBoundary);
    assert(platform.lastBoundaryApplyInfo_.hasAnyWork());
    assert(platform.lastBoundaryApplyInfo_.isLocalPaintRoot);
    assert(platform.lastBoundaryApplyPlan_.paintRoot == rootBoundary);
    assert(platform.lastBoundaryApplyPlan_.isLocalizedFor(rootBoundary));
    assert(platform.lastBoundaryApplyPlan_.hasBoundaryApplyWork(rootBoundary));
    assert(SceneTestAccess::director(scene).firstPendingBoundary() == 0);

    scene.unmount();
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;
    using loka::dsl::testing::SceneTestAccess;

    Scene scene((BoundaryDefinition<PendingApplyProbeBoundaryProps, PendingApplyProbeBoundaryNode>()));
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    BoundaryNode *rootBoundary = SceneTestAccess::rootBoundary(scene);
    assert(rootBoundary != 0);
    rootBoundary->noteOpaquePaintCoverage(true);
    scene.requestInvalidate(NODE_DIRTY_PROPS);
    assert(scene.flushInvalidation());
    assert(SceneTestAccess::lastApplyPlan(scene).paintKind == PlatformApplyPlan::PAINT_LOCAL_OPAQUE);

    scene.unmount();
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;
    using loka::dsl::testing::SceneTestAccess;

    g_pendingApplySiblingACalls = 0;
    g_pendingApplySiblingBCalls = 0;
    g_pendingApplySiblingALayoutRoot = 0;
    g_pendingApplySiblingAPaintRoot = 0;
    g_pendingApplySiblingBLayoutRoot = 0;
    g_pendingApplySiblingBPaintRoot = 0;

    Scene scene((BoundaryDefinition<PendingApplySiblingsRootProps, PendingApplySiblingsRootNode>()));
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    BoundaryNode *rootBoundary = SceneTestAccess::rootBoundary(scene);
    assert(rootBoundary != 0);
    INestable *rootNestable = rootBoundary->asNestable();
    assert(rootNestable != 0);
    Node *stackNode = rootNestable->childrenHead();
    assert(stackNode != 0);
    INestable *stackNestable = stackNode->asNestable();
    assert(stackNestable != 0);
    loka::dsl::CompositionCursor<Node> it(stackNestable->childrenHead(), stackNestable->childrenCount());
    Node *siblingANode = it.next();
    Node *siblingBNode = it.next();
    BoundaryNode *siblingA = siblingANode ? siblingANode->asBoundary() : 0;
    BoundaryNode *siblingB = siblingBNode ? siblingBNode->asBoundary() : 0;
    assert(siblingA != 0);
    assert(siblingB != 0);

    siblingA->markViewDirty(NODE_DIRTY_PROPS);
    siblingB->markViewDirty(NODE_DIRTY_PROPS);

    const SceneDirector &director = SceneTestAccess::director(scene);
    assert(director.firstPendingBoundary() != 0);
    assert(scene.flushInvalidation());
    assert(g_pendingApplySiblingACalls == 1);
    assert(g_pendingApplySiblingBCalls == 1);
    assert(platform.boundaryApplyCalls_ == 2);
    assert(g_pendingApplySiblingALayoutRoot != 0);
    assert(g_pendingApplySiblingAPaintRoot != 0);
    assert(g_pendingApplySiblingBLayoutRoot != 0);
    assert(g_pendingApplySiblingBPaintRoot != 0);
    assert(platform.lastBoundaryApplyBoundary_ == siblingB || platform.lastBoundaryApplyBoundary_ == siblingA);
    assert(SceneTestAccess::director(scene).firstPendingBoundary() == 0);

    scene.unmount();
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;
    using loka::dsl::testing::SceneTestAccess;

    g_pendingApplyDefersSiblingACalls = 0;
    g_pendingApplyDefersSiblingBCalls = 0;
    g_pendingApplyDefersSiblingAQueuedSiblingB = false;
    g_pendingApplyDeferredSiblingB = 0;

    Scene scene((BoundaryDefinition<PendingApplyDefersSiblingsRootProps, PendingApplyDefersSiblingsRootNode>()));
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    BoundaryNode *rootBoundary = SceneTestAccess::rootBoundary(scene);
    assert(rootBoundary != 0);
    INestable *rootNestable = rootBoundary->asNestable();
    assert(rootNestable != 0);
    Node *stackNode = rootNestable->childrenHead();
    assert(stackNode != 0);
    INestable *stackNestable = stackNode->asNestable();
    assert(stackNestable != 0);
    loka::dsl::CompositionCursor<Node> it(stackNestable->childrenHead(), stackNestable->childrenCount());
    Node *siblingANode = it.next();
    Node *siblingBNode = it.next();
    assert(siblingANode != 0);
    assert(siblingBNode != 0);
    BoundaryNode *siblingA = siblingANode->asBoundary();
    BoundaryNode *siblingB = siblingBNode->asBoundary();
    assert(siblingA != 0);
    assert(siblingB != 0);
    g_pendingApplyDeferredSiblingB = siblingB;

    siblingA->markViewDirty(NODE_DIRTY_PROPS);

    assert(scene.flushInvalidation());
    assert(g_pendingApplyDefersSiblingACalls == 1);
    assert(g_pendingApplyDefersSiblingBCalls == 0);
    assert(SceneTestAccess::director(scene).firstPendingBoundary() == siblingB);
    assert(scene.hasPendingInvalidation());

    assert(scene.flushInvalidation());
    assert(g_pendingApplyDefersSiblingACalls == 1);
    assert(g_pendingApplyDefersSiblingBCalls == 1);
    assert(SceneTestAccess::director(scene).firstPendingBoundary() == 0);

    scene.unmount();
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;

    loka::core::MutableState<bool> showState(false);

    NodeComposition composition;
    BoxDefinition &root = composition.declare(Box().testId("RootBox"));
    TextDefinition falseText = Text("Off").testId("OffText");
    TextDefinition trueText = Text("On").testId("OnText");
    root << composition.conditional(showState, trueText, falseText);

    Scene scene(composition.root()->clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    Scene *scenePtr = &scene;

    loka::dsl::FlowChain<Scene *, Scene *> chain =
        loka::dsl::Flow() | loka::dsl::Step(1, loka::dsl::testing::CheckText("OffText", "Off")).input(&scenePtr)
        | loka::dsl::Step(2, loka::dsl::testing::SetBoolStateAndFlush(&showState, true))
        | loka::dsl::Step(3, loka::dsl::testing::CheckText("OnText", "On"));

    assert(chain.run());
    assert((platform.lastFlags_ & loka::app::scene::NODE_DIRTY_CHILD) != 0);

    scene.unmount();
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;

    loka::core::MutableState<bool> showState(false);

    NodeComposition composition;
    BoxDefinition &root = composition.declare(Box().testId("RootBox"));
    TextDefinition falseText = Text("Off").testId("OffText");
    TextDefinition trueText = Text("On").testId("OnText");
    root << composition.conditional(showState, trueText, falseText);

    Scene scene(composition.root()->clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    Scene *scenePtr = &scene;
    loka::dsl::SnapRecord captured;

    loka::dsl::FlowChain<Scene *, loka::dsl::SnapRecord> okChain =
        loka::dsl::Flow() | loka::dsl::Step(1, loka::dsl::testing::CheckText("OffText", "Off")).input(&scenePtr)
        | loka::dsl::Step(2, loka::dsl::testing::SetBoolStateAndFlush(&showState, true))
        | loka::dsl::Step(3, loka::dsl::testing::CheckText("OnText", "On"))
        | loka::dsl::Step(4, FlowTestPlatformDirtyMaskAdapter("platform-child-dirty", &platform, 16))
              .onSuccess(&captured)
        | loka::dsl::Step(5,
                          loka::dsl::testing::AssertSnapIntMaskHasBits(
                              "platform.dirty.mask",
                              static_cast<loka::app::scene::NodeDirtyFlags>(loka::app::scene::NODE_DIRTY_CHILD
                                                                            | loka::app::scene::NODE_DIRTY_LAYOUT)))
        | loka::dsl::Step(6, loka::dsl::testing::AssertSnapIntEquals("platform.materialized", 1));

    assert(okChain.run());
    long dirtyMask = 0;
    assert(captured.getInt("platform.dirty.mask", dirtyMask));
    assert((dirtyMask & loka::app::scene::NODE_DIRTY_CHILD) != 0);
    assert((dirtyMask & loka::app::scene::NODE_DIRTY_LAYOUT) != 0);
    long fullRebuild = 0;
    assert(captured.getInt("platform.full_rebuild", fullRebuild));
    long platformCalls = 0;
    assert(captured.getInt("platform.calls", platformCalls));
    assert(platformCalls >= 1);

    scene.unmount();
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;

    loka::core::MutableState<int> fontSizeState(12);

    NodeComposition composition;
    BoxDefinition &root = composition.declare(Box().testId("RootBox"));
    root << Text("Sized").attr(TextAttr().fontSize(&fontSizeState)).testId("SizedText");

    Scene scene(composition.root()->clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    Scene *scenePtr = &scene;
    loka::dsl::SnapRecord captured;

    loka::dsl::FlowChain<Scene *, loka::dsl::SnapRecord> okChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::testing::SetIntStateAndFlush(&fontSizeState, 20)).input(&scenePtr)
        | loka::dsl::Step(2, loka::dsl::testing::CheckText("SizedText", "Sized"))
        | loka::dsl::Step(3, FlowTestPlatformDirtyMaskAdapter("platform-layout-dirty", &platform, 32))
              .onSuccess(&captured)
        | loka::dsl::Step(
            4, loka::dsl::testing::AssertSnapIntMaskHasBits("platform.dirty.mask", loka::app::scene::NODE_DIRTY_LAYOUT))
        | loka::dsl::Step(5, loka::dsl::testing::AssertSnapIntEquals("platform.materialized", 1));

    assert(okChain.run());
    long dirtyMask = 0;
    assert(captured.getInt("platform.dirty.mask", dirtyMask));
    assert((dirtyMask & loka::app::scene::NODE_DIRTY_LAYOUT) != 0);
    long fullRebuild = 1;
    assert(captured.getInt("platform.full_rebuild", fullRebuild));
    assert(fullRebuild == 0);
    long platformCalls = 0;
    assert(captured.getInt("platform.calls", platformCalls));
    assert(platformCalls >= 1);

    scene.unmount();
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;

    loka::core::MutableState<int> fontSizeState(12);

    NodeComposition composition;
    BoxDefinition &root = composition.declare(Box().testId("RootBox"));
    root << Text("Sized").attr(TextAttr().fontSize(&fontSizeState)).testId("SizedText");

    Scene scene(composition.root()->clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    Scene *scenePtr = &scene;

    loka::dsl::FlowChain<Scene *, Scene *> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::testing::SetIntStateAndFlush(&fontSizeState, 20)).input(&scenePtr)
        | loka::dsl::Step(2, loka::dsl::testing::CheckText("SizedText", "Sized"));

    assert(chain.run());
    assert((platform.lastFlags_ & loka::app::scene::NODE_DIRTY_LAYOUT) != 0);

    scene.unmount();
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;

    loka::core::MutableState<loka::core::String> textState(loka::core::String::Literal("Before"));
    loka::core::MutableState<int> fontSizeState(12);

    NodeComposition composition;
    BoxDefinition &root = composition.declare(Box().testId("RootBox"));
    root << Text(&textState).attr(TextAttr().fontSize(&fontSizeState)).testId("MixedText");

    Scene scene(composition.root()->clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    Scene *scenePtr = &scene;

    loka::dsl::FlowChain<Scene *, Scene *> chain =
        loka::dsl::Flow() | loka::dsl::Step(1, loka::dsl::testing::SetStringState(&textState, "After")).input(&scenePtr)
        | loka::dsl::Step(2, loka::dsl::testing::SetIntState(&fontSizeState, 18))
        | loka::dsl::Step(3, loka::dsl::testing::FlushSceneInvalidation())
        | loka::dsl::Step(4, loka::dsl::testing::CheckText("MixedText", "After"));

    assert(chain.run());
    assert((platform.lastFlags_ & loka::app::scene::NODE_DIRTY_LAYOUT) != 0);

    scene.unmount();
  }

  {
    const int input = 0;

    loka::dsl::FlowChain<int, loka::dsl::SnapRecord> okChain =
        loka::dsl::Flow()
        | loka::dsl::Step(
              1,
              FlowTestSnapDirtyMaskAdapter(
                  "dirty-mask-ok", loka::app::scene::NODE_DIRTY_LAYOUT | loka::app::scene::NODE_DIRTY_PROPS, 12))
              .input(&input)
        | loka::dsl::Step(2, loka::dsl::testing::CheckDirtyHasBits(loka::app::scene::NODE_DIRTY_LAYOUT));

    assert(okChain.run());

    FlowErrorCapture failCapture = {0, 0, 0};
    loka::dsl::FlowChain<int, loka::dsl::SnapRecord> failChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, FlowTestSnapDirtyMaskAdapter("dirty-mask-fail", loka::app::scene::NODE_DIRTY_PROPS, 13))
              .input(&input)
        | loka::dsl::Step(2, loka::dsl::testing::CheckDirtyHasBits(loka::app::scene::NODE_DIRTY_LAYOUT))
              .onFailure(&FlowTestMarker::captureFailure, &failCapture);

    assert(failChain.run());
    assert(failCapture.calls == 1);
    assert(failCapture.kind == loka::dsl::testing::FLOW_ERROR_KIND_SCENE_TEST_ASSERT);
    assert(failCapture.code == loka::dsl::testing::FLOW_ERROR_SCENE_TEST_ASSERTION_FAILED);
  }

  {
    const int input = 0;

    loka::dsl::FlowChain<int, loka::dsl::SnapRecord> okChain =
        loka::dsl::Flow() | loka::dsl::Step(1, FlowTestSnapDirtyMaskAdapter("dirty-equals-ok", 0, 14)).input(&input)
        | loka::dsl::Step(2, loka::dsl::testing::CheckDirtyEquals(0));

    assert(okChain.run());

    FlowErrorCapture failCapture = {0, 0, 0};
    loka::dsl::FlowChain<int, loka::dsl::SnapRecord> failChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, FlowTestSnapDirtyMaskAdapter("dirty-equals-fail", loka::app::scene::NODE_DIRTY_PROPS, 15))
              .input(&input)
        | loka::dsl::Step(2, loka::dsl::testing::CheckDirtyEquals(0))
              .onFailure(&FlowTestMarker::captureFailure, &failCapture);

    assert(failChain.run());
    assert(failCapture.calls == 1);
    assert(failCapture.kind == loka::dsl::testing::FLOW_ERROR_KIND_SCENE_TEST_ASSERT);
    assert(failCapture.code == loka::dsl::testing::FLOW_ERROR_SCENE_TEST_ASSERTION_FAILED);
  }

  {
    const int input = 0;

    loka::dsl::FlowChain<int, loka::dsl::SnapRecord> okChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, FlowTestSnapTextValueAdapter("snap-check-ok", "Hello Flow", 2)).input(&input)
        | loka::dsl::Step(2, loka::dsl::testing::CheckSnapStringEquals("text.value", "Hello Flow"));

    assert(okChain.run());

    FlowErrorCapture failCapture = {0, 0, 0};
    loka::dsl::FlowChain<int, loka::dsl::SnapRecord> failChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, FlowTestSnapTextValueAdapter("snap-check-fail", "Hello Flow", 3)).input(&input)
        | loka::dsl::Step(2, loka::dsl::testing::CheckSnapStringEquals("text.value", "Mismatch"))
              .onFailure(&FlowTestMarker::captureFailure, &failCapture);

    assert(failChain.run());
    assert(failCapture.calls == 1);
    assert(failCapture.kind == loka::dsl::testing::FLOW_ERROR_KIND_SCENE_TEST_ASSERT);
    assert(failCapture.code == loka::dsl::testing::FLOW_ERROR_SCENE_TEST_ASSERTION_FAILED);
  }

  {
    const int input = 0;
    loka::dsl::SnapRecord captured;

    loka::dsl::FlowChain<int, loka::dsl::SnapRecord> okChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::SnapV1("SceneFlow", "timing-assert-ok", "TimingNode", 4, 1).timingFlushMs(4))
              .input(&input)
        | loka::dsl::Step(2, loka::dsl::testing::AssertSnapIntLessEqual("timing.flush_ms", 5)).onSuccess(&captured);

    assert(okChain.run());
    long flushMs = 0;
    assert(captured.getInt("timing.flush_ms", flushMs));
    assert(flushMs == 4);

    FlowErrorCapture failCapture = {0, 0, 0};
    loka::dsl::FlowChain<int, loka::dsl::SnapRecord> failChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::SnapV1("SceneFlow", "timing-assert-fail", "TimingNode", 5, 1).timingFlushMs(7))
              .input(&input)
        | loka::dsl::Step(2, loka::dsl::testing::AssertSnapIntLessEqual("timing.flush_ms", 5))
              .onFailure(&FlowTestMarker::captureFailure, &failCapture);

    assert(failChain.run());
    assert(failCapture.calls == 1);
    assert(failCapture.kind == loka::dsl::testing::FLOW_ERROR_KIND_SCENE_TEST_ASSERT);
    assert(failCapture.code == loka::dsl::testing::FLOW_ERROR_SCENE_TEST_ASSERTION_FAILED);
  }

  {
    const int input = 0;

    loka::dsl::FlowChain<int, loka::dsl::SnapRecord> okChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::SnapV1("SceneFlow", "int-gte-ok", "LayoutNode", 8, 1).timingLayoutMs(12))
              .input(&input)
        | loka::dsl::Step(2, loka::dsl::testing::CheckSnapIntGreaterEqual("timing.layout_ms", 10));

    assert(okChain.run());

    FlowErrorCapture failCapture = {0, 0, 0};
    loka::dsl::FlowChain<int, loka::dsl::SnapRecord> failChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::SnapV1("SceneFlow", "int-gte-fail", "LayoutNode", 9, 1).timingLayoutMs(7))
              .input(&input)
        | loka::dsl::Step(2, loka::dsl::testing::CheckSnapIntGreaterEqual("timing.layout_ms", 10))
              .onFailure(&FlowTestMarker::captureFailure, &failCapture);

    assert(failChain.run());
    assert(failCapture.calls == 1);
    assert(failCapture.kind == loka::dsl::testing::FLOW_ERROR_KIND_SCENE_TEST_ASSERT);
    assert(failCapture.code == loka::dsl::testing::FLOW_ERROR_SCENE_TEST_ASSERTION_FAILED);
  }

  {
    const int input = 0;

    loka::dsl::FlowChain<int, loka::dsl::SnapRecord> okChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::SnapV1("SceneFlow", "timing-check-ok", "TimingNode", 6, 1).timingFlushMs(4))
              .input(&input)
        | loka::dsl::Step(2, loka::dsl::testing::CheckTimingLessEqual("timing.flush_ms", 5));

    assert(okChain.run());

    FlowErrorCapture failCapture = {0, 0, 0};
    loka::dsl::FlowChain<int, loka::dsl::SnapRecord> failChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::SnapV1("SceneFlow", "timing-check-fail", "TimingNode", 7, 1).timingFlushMs(8))
              .input(&input)
        | loka::dsl::Step(2, loka::dsl::testing::CheckTimingLessEqual("timing.flush_ms", 5))
              .onFailure(&FlowTestMarker::captureFailure, &failCapture);

    assert(failChain.run());
    assert(failCapture.calls == 1);
    assert(failCapture.kind == loka::dsl::testing::FLOW_ERROR_KIND_SCENE_TEST_ASSERT);
    assert(failCapture.code == loka::dsl::testing::FLOW_ERROR_SCENE_TEST_ASSERTION_FAILED);
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;

    NodeComposition composition;
    BoxDefinition &root = composition.declare(Box().testId("RootBox"));
    root << Text("Hello Flow").testId("MainText");

    Scene scene(composition.root()->clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    Scene *scenePtr = &scene;
    loka::dsl::SnapRecord captured;

    loka::dsl::FlowChain<Scene *, loka::dsl::SnapRecord> okChain =
        loka::dsl::Flow() | loka::dsl::Step(1, loka::dsl::testing::FindNodeById<TextNode>("MainText")).input(&scenePtr)
        | loka::dsl::Step(2, loka::dsl::testing::CaptureNode<TextNode>("SceneFlow", "capture-assert", 2, 1))
        | loka::dsl::Step(3, loka::dsl::testing::AssertSnapStringEquals("text.value", "Hello Flow"))
              .onSuccess(&captured);

    assert(okChain.run());
    std::string value;
    assert(captured.get("text.value", value));
    assert(value == "Hello Flow");

    FlowErrorCapture failCapture = {0, 0, 0};
    loka::dsl::FlowChain<Scene *, loka::dsl::SnapRecord> failChain =
        loka::dsl::Flow() | loka::dsl::Step(1, loka::dsl::testing::FindNodeById<TextNode>("MainText")).input(&scenePtr)
        | loka::dsl::Step(2, loka::dsl::testing::CaptureNode<TextNode>("SceneFlow", "capture-assert-fail", 3, 1))
        | loka::dsl::Step(3, loka::dsl::testing::AssertSnapStringEquals("text.value", "Mismatch"))
              .onFailure(&FlowTestMarker::captureFailure, &failCapture);

    assert(failChain.run());
    assert(failCapture.calls == 1);
    assert(failCapture.kind == loka::dsl::testing::FLOW_ERROR_KIND_SCENE_TEST_ASSERT);
    assert(failCapture.code == loka::dsl::testing::FLOW_ERROR_SCENE_TEST_ASSERTION_FAILED);

    scene.unmount();
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;

    NodeComposition composition;
    BoxDefinition &root = composition.declare(Box().testId("RootBox"));
    root << Text("Ready").testId("StatusText");

    Scene scene(composition.root()->clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    Scene *scenePtr = &scene;
    Scene *sameScene = 0;

    loka::dsl::FlowChain<Scene *, Scene *> okChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::testing::CheckText("StatusText", "Ready"))
              .input(&scenePtr)
              .onSuccess(&sameScene);

    assert(okChain.run());
    assert(sameScene == scenePtr);

    FlowErrorCapture failCapture = {0, 0, 0};
    loka::dsl::FlowChain<Scene *, Scene *> failChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::testing::CheckText("StatusText", "Busy"))
              .input(&scenePtr)
              .onFailure(&FlowTestMarker::captureFailure, &failCapture);

    assert(failChain.run());
    assert(failCapture.calls == 1);
    assert(failCapture.kind == loka::dsl::testing::FLOW_ERROR_KIND_SCENE_TEST_ASSERT);
    assert(failCapture.code == loka::dsl::testing::FLOW_ERROR_SCENE_TEST_ASSERTION_FAILED);

    scene.unmount();
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;

    NodeComposition composition;
    BoxDefinition &root = composition.declare(Box().testId("RootBox"));
    root << Text("Ready").testId("StatusText");

    Scene scene(composition.root()->clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    Scene *scenePtr = &scene;

    loka::dsl::FlowChain<Scene *, TextNode *> okChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::testing::FindNodeById<TextNode>("StatusText")).input(&scenePtr)
        | loka::dsl::Step(2, loka::dsl::testing::AssertTextEquals("Ready"));

    assert(okChain.run());

    FlowErrorCapture capture = {0, 0, 0};
    loka::dsl::FlowChain<Scene *, TextNode *> failChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::testing::FindNodeById<TextNode>("StatusText")).input(&scenePtr)
        | loka::dsl::Step(2, loka::dsl::testing::AssertTextEquals("Busy"))
              .onFailure(&FlowTestMarker::captureFailure, &capture);

    assert(failChain.run());
    assert(capture.calls == 1);
    assert(capture.kind == loka::dsl::testing::FLOW_ERROR_KIND_SCENE_TEST_ASSERT);
    assert(capture.code == loka::dsl::testing::FLOW_ERROR_SCENE_TEST_ASSERTION_FAILED);

    scene.unmount();
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;

    NodeComposition composition;
    BoxDefinition &root = composition.declare(Box().testId("RootBox"));
    root << Button("Press").testId("ActionButton");

    Scene scene(composition.root()->clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    Scene *scenePtr = &scene;
    FlowErrorCapture capture = {0, 0, 0};

    loka::dsl::FlowChain<Scene *, TextNode *> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::testing::FindNodeById<TextNode>("ActionButton"))
              .input(&scenePtr)
              .onFailure(&FlowTestMarker::captureFailure, &capture);

    assert(chain.run());
    assert(capture.calls == 1);
    assert(capture.kind == loka::dsl::testing::FLOW_ERROR_KIND_SCENE_SCENARIO);
    assert(capture.code == loka::dsl::testing::FLOW_ERROR_SCENE_TEST_NODE_TYPE_MISMATCH);

    scene.unmount();
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;

    NodeComposition composition;
    BoxDefinition &root = composition.declare(Box().testId("RootBox"));
    root << Text("A").testId("DupText");
    root << Text("B").testId("DupText");

    Scene scene(composition.root()->clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    Scene *scenePtr = &scene;
    FlowErrorCapture capture = {0, 0, 0};

    loka::dsl::FlowChain<Scene *, TextNode *> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::testing::FindNodeById<TextNode>("DupText"))
              .input(&scenePtr)
              .onFailure(&FlowTestMarker::captureFailure, &capture);

    assert(chain.run());
    assert(capture.calls == 1);
    assert(capture.kind == loka::dsl::testing::FLOW_ERROR_KIND_SCENE_SCENARIO);
    assert(capture.code == loka::dsl::testing::FLOW_ERROR_SCENE_TEST_DUPLICATE_TEST_ID);

    scene.unmount();
  }

  {
    loka::app::FileChooserResult fileResult;
    fileResult.kind = loka::app::FileChooserResult::RESULT_FILE;
    fileResult.item = loka::file::File::FromPath(loka::core::String::Literal("C:/tmp/a.png"));
    simpleviewer::ChooserContext context;
    simpleviewer::ChooserProjection projection;

    loka::dsl::FlowChain<loka::app::FileChooserResult, simpleviewer::ChooserProjection> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, simpleviewer::ChooserToContextAdapter()).input(&fileResult).onSuccess(&context)
        | loka::dsl::Step(2, simpleviewer::ContextToProjectionAdapter()).onSuccess(&projection);

    assert(chain.run());
    assert(projection.request.source == loka::core::resource::BLOB_SOURCE_FILE);
    assert(!projection.request.filePath.empty());
    assert(projection.request.tag.equals(loka::core::String::Literal("image-file")));
    assert(projection.message.equals(loka::core::String::Literal("Loka file: C:/tmp/a.png")));
  }

  {
    loka::app::FileChooserResult canceled;
    canceled.kind = loka::app::FileChooserResult::RESULT_CANCELED;
    simpleviewer::ChooserContext context;
    simpleviewer::ChooserProjection projection;

    loka::dsl::FlowChain<loka::app::FileChooserResult, simpleviewer::ChooserProjection> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, simpleviewer::ChooserToContextAdapter()).input(&canceled).onSuccess(&context)
        | loka::dsl::Step(2, simpleviewer::ContextToProjectionAdapter()).onSuccess(&projection);

    assert(chain.run());
    assert(projection.request.source == loka::core::resource::BLOB_SOURCE_NONE);
    assert(projection.message.equals(loka::core::String::Literal("Canceled")));
  }

  {
    loka::app::FileChooserResult folder =
        loka::app::FileChooserResult::Folder(loka::file::File::FromPath(loka::core::String::Literal("C:/tmp/images")));
    simpleviewer::ChooserContext context;
    simpleviewer::ChooserProjection projection;

    loka::dsl::FlowChain<loka::app::FileChooserResult, simpleviewer::ChooserProjection> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, simpleviewer::ChooserToContextAdapter()).input(&folder).onSuccess(&context)
        | loka::dsl::Step(2, simpleviewer::ContextToProjectionAdapter()).onSuccess(&projection);

    assert(chain.run());
    assert(projection.request.source == loka::core::resource::BLOB_SOURCE_NONE);
    assert(projection.request.filePath.empty());
    assert(projection.message.equals(loka::core::String::Literal("Loka folder: C:/tmp/images")));
  }

  {
    loka::app::FileChooserResult errorResult = loka::app::FileChooserResult::Error(42);
    simpleviewer::ChooserContext context;
    simpleviewer::ChooserProjection projection;

    loka::dsl::FlowChain<loka::app::FileChooserResult, simpleviewer::ChooserProjection> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, simpleviewer::ChooserToContextAdapter()).input(&errorResult).onSuccess(&context)
        | loka::dsl::Step(2, simpleviewer::ContextToProjectionAdapter()).onSuccess(&projection);

    assert(chain.run());
    assert(projection.request.source == loka::core::resource::BLOB_SOURCE_NONE);
    assert(projection.request.filePath.empty());
    assert(projection.message.equals(loka::core::String::Literal("Error 42")));
  }

  {
    loka::app::FileChooserResult fileEmptyPath;
    fileEmptyPath.kind = loka::app::FileChooserResult::RESULT_FILE;
    simpleviewer::ChooserContext context;
    simpleviewer::ChooserProjection projection;

    loka::dsl::FlowChain<loka::app::FileChooserResult, simpleviewer::ChooserProjection> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, simpleviewer::ChooserToContextAdapter()).input(&fileEmptyPath).onSuccess(&context)
        | loka::dsl::Step(2, simpleviewer::ContextToProjectionAdapter()).onSuccess(&projection);

    assert(chain.run());
    assert(projection.request.source == loka::core::resource::BLOB_SOURCE_NONE);
    assert(projection.request.filePath.empty());
    assert(projection.request.tag.empty());
    assert(projection.message.equals(loka::core::String::Literal("Loka file: (unknown)")));
  }

  {
    loka::core::resource::Blob blob = loka::core::resource::Blob::Create();
    simpleviewer::BlobDecodeAttempt attempt;
    loka::core::resource::Image image;
    FlowErrorCapture capture = {0, 0, 0};

    loka::dsl::FlowChain<loka::core::resource::Blob, loka::core::resource::Image> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, simpleviewer::BlobToDecodeAttemptAdapter(0))
              .input(&blob)
              .onSuccess(&attempt)
              .onFailure(&FlowTestMarker::captureFailure, &capture)
        | loka::dsl::Step(2, simpleviewer::DecodeAttemptToImageAdapter()).onSuccess(&image);

    assert(chain.run());
    assert(!image.isValid());
    assert(capture.calls == 1);
    assert(capture.kind == simpleviewer::SIMPLE_VIEWER_FLOW_ERROR_DECODE);
    assert(capture.code == simpleviewer::SIMPLE_VIEWER_FLOW_ERROR_CODE_PLATFORM_CONTEXT_MISSING);
  }

  {
    FlowTestPlatformContext ctx;
    ctx.createImageResult_ = true;
    loka::core::resource::Blob blob = loka::core::resource::Blob::Create();
    simpleviewer::BlobDecodeAttempt attempt;
    loka::core::resource::Image image;
    FlowErrorCapture capture = {0, 0, 0};

    loka::dsl::FlowChain<loka::core::resource::Blob, loka::core::resource::Image> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, simpleviewer::BlobToDecodeAttemptAdapter(&ctx))
              .input(&blob)
              .onSuccess(&attempt)
              .onFailure(&FlowTestMarker::captureFailure, &capture)
        | loka::dsl::Step(2, simpleviewer::DecodeAttemptToImageAdapter()).onSuccess(&image);

    assert(chain.run());
    assert(ctx.createImageCalls_ == 1);
    assert(image.isValid());
    assert(image.width() == 16);
    assert(image.height() == 16);
    assert(capture.calls == 0);
  }

  {
    FlowTestPlatformContext ctx;
    ctx.createImageResult_ = false;
    loka::core::resource::Blob blob = loka::core::resource::Blob::Create();
    simpleviewer::BlobDecodeAttempt attempt;
    loka::core::resource::Image image;
    FlowErrorCapture capture = {0, 0, 0};

    loka::dsl::FlowChain<loka::core::resource::Blob, loka::core::resource::Image> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, simpleviewer::BlobToDecodeAttemptAdapter(&ctx))
              .input(&blob)
              .onSuccess(&attempt)
              .onFailure(&FlowTestMarker::captureFailure, &capture)
        | loka::dsl::Step(2, simpleviewer::DecodeAttemptToImageAdapter()).onSuccess(&image);

    assert(chain.run());
    assert(ctx.createImageCalls_ == 1);
    assert(!image.isValid());
    assert(capture.calls == 1);
    assert(capture.kind == simpleviewer::SIMPLE_VIEWER_FLOW_ERROR_DECODE);
    assert(capture.code == simpleviewer::SIMPLE_VIEWER_FLOW_ERROR_CODE_IMAGE_DECODE_FAILED);
  }

  {
    int input = 10;
    int calls = 0;
    bool ready = false;
    int captured = 0;

    loka::dsl::FlowChain<int, int> chain =
        loka::dsl::Flow() | loka::dsl::Step(1, FlowTestMul2Adapter()).input(&input)
        | loka::dsl::Step(2, FlowTestPendingThenSuccessAdapter(&ready, &calls)).onSuccess(&captured);

    const loka::dsl::FlowRunResult first = chain.runResult();
    assert(first == loka::dsl::FLOW_RUN_PENDING);
    assert(calls == 1);

    ready = true;
    const loka::dsl::FlowRunResult resumed = chain.resumeResult(2);
    assert(resumed == loka::dsl::FLOW_RUN_SUCCEEDED);
    assert(calls == 2);
    assert(captured == 120);
  }

  {
    loka::core::MutableState<int> trigger;
    int calls = 0;
    bool ready = false;
    int captured = 0;

    loka::dsl::FlowChain<int, int> chain =
        loka::dsl::Flow() | loka::dsl::Step(1, FlowTestPendingThenSuccessAdapter(&ready, &calls)).onSuccess(&captured);
    chain.bindTrigger(&trigger);

    trigger.set(9, true);
    assert(calls == 1);
    assert(captured == 0);

    chain.cancel();
    ready = true;
    const loka::dsl::FlowRunResult canceled = chain.resumeResult(1);
    assert(canceled == loka::dsl::FLOW_RUN_CANCELED);
    assert(calls == 1);
    assert(captured == 0);

    // Cancel terminal must not consume pending step output.
    assert(calls == 1);
    assert(captured == 0);
  }

  {
    int input = 5;
    loka::core::MutableState<int> result;

    loka::dsl::FlowChain<int, int> chain =
        loka::dsl::Flow() | loka::dsl::Step(1, FlowTestMul2Adapter()).input(&input).onSuccess(&result);

    assert(chain.run());
    assert(result.get() == 10);
  }

  {
    int input = 7;
    loka::core::MutableState<int> result;
    loka::core::PushStateTracker tracker;
    tracker.addState(&result);

    loka::dsl::FlowChain<int, int> chain =
        loka::dsl::Flow() | loka::dsl::Step(1, FlowTestMul2Adapter()).input(&input).onSuccess(&result);
    chain.withTracker(&tracker);

    assert(chain.run());
    assert(result.get() == 14);
  }

  {
    int input = 1;
    int callsA = 0;
    std::vector<int> order;
    FlowTestMarkerContext flowFinal = {&order, 1599};

    loka::dsl::FlowChain<int, int> chain =
        loka::dsl::Flow() | loka::dsl::Step(1, FlowTestCountedPassAdapter(&callsA)).input(&input).onSuccess(&input, 1);
    chain.onFinally(&FlowTestMarker::onStepFinally, &flowFinal);

    const loka::dsl::FlowRunResult result = chain.runResult();
    assert(result == loka::dsl::FLOW_RUN_FAILED);
    assert(callsA <= 1024);
    assert(order.size() == 1);
    assert(order[0] == 1599);
  }

  // --- ProjectionToBlobAdapter: source=NONE (cancel) → empty Blob, success ---
  {
    simpleviewer::ChooserProjection projection;
    // default: source == BLOB_SOURCE_NONE
    loka::core::resource::Blob blob;
    FlowErrorCapture capture = {0, 0, 0};

    loka::dsl::FlowChain<simpleviewer::ChooserProjection, loka::core::resource::Blob> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, simpleviewer::ProjectionToBlobAdapter())
              .input(&projection)
              .onFailure(&FlowTestMarker::captureFailure, &capture)
              .onSuccess(&blob);

    assert(chain.run());
    assert(capture.calls == 1);
    assert(capture.kind == simpleviewer::SIMPLE_VIEWER_FLOW_ERROR_BLOB_LOAD);
    assert(capture.code == simpleviewer::SIMPLE_VIEWER_FLOW_ERROR_CODE_NO_FILE_SELECTED);
  }

  // --- ProjectionToBlobAdapter: source=FILE → reads bytes from file ---
  {
    const char *tmpPath = "_loka_test_blob_adapter.bin";
    {
      const unsigned char data[] = {0xDE, 0xAD, 0xBE, 0xEF};
      FILE *out = std::fopen(tmpPath, "wb");
      assert(out != 0);
      const std::size_t written = std::fwrite(data, 1, sizeof(data), out);
      assert(written == sizeof(data));
      assert(std::fclose(out) == 0);
    }

    simpleviewer::ChooserProjection projection;
    projection.request.setFilePath(loka::core::String::Literal(tmpPath));

    loka::core::resource::Blob blob;
    FlowErrorCapture capture = {0, 0, 0};

    loka::dsl::FlowChain<simpleviewer::ChooserProjection, loka::core::resource::Blob> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, simpleviewer::ProjectionToBlobAdapter())
              .input(&projection)
              .onSuccess(&blob)
              .onFailure(&FlowTestMarker::captureFailure, &capture);

    assert(chain.run());
    assert(capture.calls == 0);
    assert(blob.isValid());
    assert(blob.bytes().size() == 4);
    assert(blob.bytes()[0] == 0xDE);
    assert(blob.bytes()[1] == 0xAD);
    assert(blob.bytes()[2] == 0xBE);
    assert(blob.bytes()[3] == 0xEF);
    assert(blob.isCompleted());

    std::remove(tmpPath);
  }

  // --- ProjectionToBlobAdapter: source=FILE, missing file → failure ---
  {
    simpleviewer::ChooserProjection projection;
    projection.request.setFilePath(loka::core::String::Literal("_loka_nonexistent_file.bin"));

    loka::core::resource::Blob blob;
    FlowErrorCapture capture = {0, 0, 0};

    loka::dsl::FlowChain<simpleviewer::ChooserProjection, loka::core::resource::Blob> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, simpleviewer::ProjectionToBlobAdapter())
              .input(&projection)
              .onFailure(&FlowTestMarker::captureFailure, &capture)
              .onSuccess(&blob);

    assert(chain.run());
    assert(capture.calls == 1);
    assert(capture.kind == simpleviewer::SIMPLE_VIEWER_FLOW_ERROR_BLOB_LOAD);
    assert(capture.code == simpleviewer::SIMPLE_VIEWER_FLOW_ERROR_CODE_STDIO_OPEN_FAILED);
  }

  // --- Full 6-step integrated chain: FileChooserResult → Image ---
  {
    const char *tmpPath = "_loka_test_full_chain.bin";
    {
      const unsigned char data[] = {0x89, 0x50, 0x4E, 0x47}; // fake PNG header
      FILE *out = std::fopen(tmpPath, "wb");
      assert(out != 0);
      const std::size_t written = std::fwrite(data, 1, sizeof(data), out);
      assert(written == sizeof(data));
      assert(std::fclose(out) == 0);
    }

    FlowTestPlatformContext ctx;
    ctx.createImageResult_ = true;

    loka::app::FileChooserResult fileResult;
    fileResult.kind = loka::app::FileChooserResult::RESULT_FILE;
    fileResult.item = loka::file::File::FromPath(loka::core::String::Literal(tmpPath));

    loka::core::resource::Image image;
    FlowErrorCapture capture = {0, 0, 0};

    loka::dsl::FlowChain<loka::app::FileChooserResult, loka::core::resource::Image> chain =
        loka::dsl::Flow() | loka::dsl::Step(1, simpleviewer::ChooserToContextAdapter()).input(&fileResult)
        | loka::dsl::Step(2, simpleviewer::ContextToProjectionAdapter())
        | loka::dsl::Step(3, simpleviewer::ProjectionToBlobAdapter())
        | loka::dsl::Step(4, simpleviewer::BlobToDecodeAttemptAdapter(&ctx))
              .onFailure(&FlowTestMarker::captureFailure, &capture)
        | loka::dsl::Step(5, simpleviewer::DecodeAttemptToImageAdapter()).onSuccess(&image);

    assert(chain.run());
    assert(capture.calls == 0);
    assert(ctx.createImageCalls_ == 1);
    assert(image.isValid());
    assert(image.width() == 16);
    assert(image.height() == 16);

    std::remove(tmpPath);
  }

  // --- Full 6-step chain: canceled → no-file-selected at blob projection ---
  {
    FlowTestPlatformContext ctx;
    ctx.createImageResult_ = false;

    loka::app::FileChooserResult canceled;
    canceled.kind = loka::app::FileChooserResult::RESULT_CANCELED;

    loka::core::resource::Image image;
    FlowErrorCapture capture = {0, 0, 0};

    loka::dsl::FlowChain<loka::app::FileChooserResult, loka::core::resource::Image> chain =
        loka::dsl::Flow() | loka::dsl::Step(1, simpleviewer::ChooserToContextAdapter()).input(&canceled)
        | loka::dsl::Step(2, simpleviewer::ContextToProjectionAdapter())
        | loka::dsl::Step(3, simpleviewer::ProjectionToBlobAdapter())
        | loka::dsl::Step(4, simpleviewer::BlobToDecodeAttemptAdapter(&ctx))
              .onFailure(&FlowTestMarker::captureFailure, &capture)
        | loka::dsl::Step(5, simpleviewer::DecodeAttemptToImageAdapter()).onSuccess(&image);
    chain.onFailure(&FlowTestMarker::captureFailure, &capture);

    assert(chain.run());
    assert(!image.isValid());
    assert(capture.calls == 1);
    assert(capture.kind == simpleviewer::SIMPLE_VIEWER_FLOW_ERROR_BLOB_LOAD);
    assert(capture.code == simpleviewer::SIMPLE_VIEWER_FLOW_ERROR_CODE_NO_FILE_SELECTED);
  }

  // --- onSuccess field extraction: extract single field into MutableState ---
  {
    int input = 5;
    loka::core::MutableState<int> valueState;
    loka::core::MutableState<int> extraState;

    loka::dsl::FlowChain<int, FlowTestFieldOutput> chain = loka::dsl::Flow()
                                                           | loka::dsl::Step(1, FlowTestFieldAdapter())
                                                                 .input(&input)
                                                                 .onSuccess(&valueState, &FlowTestFieldOutput::value)
                                                                 .onSuccess(&extraState, &FlowTestFieldOutput::extra);

    assert(chain.run());
    assert(valueState.get() == 50); // 5 * 10
    assert(extraState.get() == 6);  // 5 + 1
  }

  // --- bindTrigger: auto-execute flow on state change ---
  {
    loka::core::MutableState<int> trigger;
    loka::core::MutableState<int> result;

    loka::dsl::FlowChain<int, int> chain =
        loka::dsl::Flow() | loka::dsl::Step(1, FlowTestMul2Adapter()).onSuccess(&result);
    chain.bindTrigger(&trigger);

    trigger.set(7, true);
    assert(result.get() == 14); // 7 * 2

    trigger.set(3, true);
    assert(result.get() == 6); // 3 * 2
  }

  // --- FlowSlot: long-lived owner forwards run and trigger binding ---
  {
    typedef loka::dsl::FlowChain<int, int> SlotFlowChain;

    int input = 8;
    int directResult = 0;
    loka::app::scene::FlowSlot<SlotFlowChain> slot;
    SlotFlowChain directChain =
        loka::dsl::Flow() | loka::dsl::Step(1, FlowTestMul2Adapter()).input(&input).onSuccess(&directResult);

    assert(!slot.isValid());
    slot.set(directChain);
    assert(slot.isValid());
    assert(slot.run());
    assert(directResult == 16);

    loka::core::MutableState<int> trigger;
    loka::core::MutableState<int> triggerResult;
    SlotFlowChain triggerChain =
        loka::dsl::Flow() | loka::dsl::Step(1, FlowTestAdd1Adapter()).onSuccess(&triggerResult);

    slot.set(triggerChain).bindTrigger(&trigger);
    trigger.set(41, true);
    assert(triggerResult.get() == 42);

    slot.clear();
    assert(!slot.isValid());
  }

  // --- FlowSlot: cancel and resume are available through the owner slot ---
  {
    typedef loka::dsl::FlowChain<int, int> SlotFlowChain;

    int input = 12;
    int calls = 0;
    bool ready = false;
    int captured = 0;
    loka::app::scene::FlowSlot<SlotFlowChain> slot;
    SlotFlowChain chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, FlowTestPendingThenSuccessAdapter(&ready, &calls)).input(&input).onSuccess(&captured);

    slot.set(chain);
    assert(slot.runResult() == loka::dsl::FLOW_RUN_PENDING);
    assert(calls == 1);
    slot.cancel();
    ready = true;
    assert(slot.resumeResult(1) == loka::dsl::FLOW_RUN_CANCELED);
    assert(captured == 0);

    slot.clear();
  }

  // --- FlowSlot: set during onSuccess defers replacing the running flow ---
  {
    typedef loka::dsl::FlowChain<int, int> SlotFlowChain;

    struct SetDuringRunHelper
    {
      struct Context
      {
        loka::app::scene::FlowSlot<SlotFlowChain> *slot;
        SlotFlowChain *replacement;
        int *calls;
      };

      static void replaceFlow(const int &, void *user)
      {
        Context *ctx = static_cast<Context *>(user);
        ++(*ctx->calls);
        ctx->slot->set(*ctx->replacement);
      }
    };

    int input = 4;
    int oldResult = 0;
    int replacementInput = 10;
    int replacementResult = 0;
    int replaceCalls = 0;
    loka::app::scene::FlowSlot<SlotFlowChain> slot;
    SlotFlowChain replacement =
        loka::dsl::Flow() | loka::dsl::Step(1, FlowTestAdd1Adapter()).input(&replacementInput).onSuccess(&replacementResult);
    SetDuringRunHelper::Context ctx = {&slot, &replacement, &replaceCalls};
    slot.set(loka::dsl::Flow()
             | loka::dsl::Step(1, FlowTestMul2Adapter())
                   .input(&input)
                   .onSuccess(&SetDuringRunHelper::replaceFlow, &ctx)
                   .onSuccess(&oldResult));
    assert(slot.runResult() == loka::dsl::FLOW_RUN_SUCCEEDED);
    assert(replaceCalls == 1);
    assert(oldResult == 8);
    assert(slot.isValid());
    assert(slot.runResult() == loka::dsl::FLOW_RUN_SUCCEEDED);
    assert(replacementResult == 11);

    slot.clear();
  }

  // --- FlowSlot: clear during onSuccess defers deleting the running flow ---
  {
    typedef loka::dsl::FlowChain<int, int> SlotFlowChain;

    struct ClearDuringRunHelper
    {
      struct Context
      {
        loka::app::scene::FlowSlot<SlotFlowChain> *slot;
        int *calls;
      };

      static void clearFlow(const int &, void *user)
      {
        Context *ctx = static_cast<Context *>(user);
        ++(*ctx->calls);
        ctx->slot->clear();
      }
    };

    int input = 5;
    int oldResult = 0;
    int clearCalls = 0;
    loka::app::scene::FlowSlot<SlotFlowChain> slot;
    ClearDuringRunHelper::Context ctx = {&slot, &clearCalls};
    slot.set(loka::dsl::Flow()
             | loka::dsl::Step(1, FlowTestMul2Adapter())
                   .input(&input)
                   .onSuccess(&ClearDuringRunHelper::clearFlow, &ctx)
                   .onSuccess(&oldResult));
    assert(slot.runResult() == loka::dsl::FLOW_RUN_SUCCEEDED);
    assert(clearCalls == 1);
    assert(oldResult == 10);
    assert(!slot.isValid());
  }

  // --- FlowSlot: a chain copied out of the slot outlives the slot safely ---
  // FlowChain copies share the impl, so a copy taken via dangerouslyUnwrap()
  // keeps the hooked impl alive after the slot is gone. Disowning must clear
  // the hooks on the shared impl, or running the escaped copy would fire
  // them into the slot's freed run state.
  {
    typedef loka::dsl::FlowChain<int, int> SlotFlowChain;

    int input = 6;
    int result = 0;
    SlotFlowChain *escaped = 0;
    {
      loka::app::scene::FlowSlot<SlotFlowChain> slot;
      slot.set(loka::dsl::Flow()
               | loka::dsl::Step(1, FlowTestMul2Adapter()).input(&input).onSuccess(&result));
      assert(slot.run());
      assert(result == 12);
      escaped = new SlotFlowChain(slot.dangerouslyUnwrap()); // shares the hooked impl
    } // slot destroyed: its run state is freed with the slot

    result = 0;
    assert(escaped->run()); // pre-fix: hooks fire into the freed run state (ASan UAF)
    assert(result == 12);
    delete escaped;
  }

  // --- FlowSlot: extending an escaped shared chain drops slot-owned hooks ---
  {
    typedef loka::dsl::FlowChain<int, int> SlotFlowChain;

    int input = 6;
    int result = 0;
    SlotFlowChain *escaped = 0;
    {
      loka::app::scene::FlowSlot<SlotFlowChain> slot;
      slot.set(loka::dsl::Flow() | loka::dsl::Step(1, FlowTestMul2Adapter()).input(&input));
      escaped = new SlotFlowChain(slot.dangerouslyUnwrap()
                                  | loka::dsl::Step(2, FlowTestAdd1Adapter()).onSuccess(&result));
    }

    assert(escaped->run());
    assert(result == 13);
    delete escaped;
  }

  // --- FlowChain: run pinning must not make cancel() detach from the active impl ---
  {
    typedef loka::dsl::FlowChain<int, int> SlotFlowChain;

    struct CancelDuringFlowSuccess
    {
      struct Context
      {
        SlotFlowChain *chain;
        int *calls;
      };

      static void cancelSelf(void *user)
      {
        Context *ctx = static_cast<Context *>(user);
        ++(*ctx->calls);
        ctx->chain->cancel();
      }
    };

    int input = 3;
    int stepCalls = 0;
    int cancelCalls = 0;
    SlotFlowChain chain = loka::dsl::Flow() | loka::dsl::Step(1, FlowTestCountedPassAdapter(&stepCalls)).input(&input);
    CancelDuringFlowSuccess::Context ctx = {&chain, &cancelCalls};
    chain.onSuccess(&CancelDuringFlowSuccess::cancelSelf, &ctx, 1);

    assert(chain.runResult() == loka::dsl::FLOW_RUN_CANCELED);
    assert(stepCalls == 1);
    assert(cancelCalls == 1);
  }

  // --- bindTrigger: reentry guard prevents double execution ---
  {
    loka::core::MutableState<int> trigger;
    int calls = 0;

    // Callback that re-sets the trigger during step onSuccess processing.
    // This fires while the flow is still running, so reentry guard must block it.
    struct ReentryHelper
    {
      static void reSetTrigger(const int &, void *user)
      {
        loka::core::MutableState<int> *t = static_cast<loka::core::MutableState<int> *>(user);
        t->set(99); // value changes → notifyStateChanged → OnTriggerChanged
      }
    };

    loka::dsl::FlowChain<int, int> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, FlowTestCountedPassAdapter(&calls)).onSuccess(&ReentryHelper::reSetTrigger, &trigger);
    chain.bindTrigger(&trigger);

    trigger.set(10);
    assert(calls == 1);          // reentry blocked: flow ran only once
    assert(trigger.get() == 99); // but the trigger value was updated
  }

  // --- bindTrigger: trigger-driven cancel must hit the active impl ---
  {
    struct CancelDuringTriggeredStep
    {
      struct Context
      {
        loka::dsl::FlowChain<int, int> *chain;
        int *cancelCalls;
      };

      static void cancelSelf(const int &, void *user)
      {
        Context *ctx = static_cast<Context *>(user);
        ++(*ctx->cancelCalls);
        ctx->chain->cancel();
      }
    };

    loka::core::MutableState<int> trigger;
    int callsA = 0;
    int callsB = 0;
    int cancelCalls = 0;
    CancelDuringTriggeredStep::Context ctx = {0, &cancelCalls};
    loka::dsl::FlowChain<int, int> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, FlowTestCountedPassAdapter(&callsA))
              .onSuccess(&CancelDuringTriggeredStep::cancelSelf, &ctx)
        | loka::dsl::Step(2, FlowTestCountedPassAdapter(&callsB));
    ctx.chain = &chain;
    chain.bindTrigger(&trigger);

    trigger.set(10);
    assert(callsA == 1);
    assert(callsB == 0);
    assert(cancelCalls == 1);
  }

  // --- State<void>: callback may delete emitter safely ---
  {
    StateNotifyDeleteCtx ctx;
    ctx.state = new loka::core::EmitterState();
    ctx.valueState = 0;
    ctx.destroyCalls = 0;
    ctx.siblingCalls = 0;

    ctx.state->bind(&StateNotifyHelpers::destroySelf, &ctx, false);
    ctx.state->bind(&StateNotifyHelpers::sibling, &ctx, false);
    ctx.state->emit();

    assert(ctx.state == 0);
    assert(ctx.destroyCalls == 1);
    assert(ctx.siblingCalls == 0);
  }

  // --- State<void>: self-unbind runs once across multiple emits ---
  {
    StateNotifyDeleteCtx ctx;
    loka::core::EmitterState state;
    ctx.state = &state;
    ctx.valueState = 0;
    ctx.destroyCalls = 0;
    ctx.siblingCalls = 0;

    state.bind(&StateNotifyHelpers::selfUnbind, &ctx, false);
    state.bind(&StateNotifyHelpers::sibling, &ctx, false);

    state.emit();
    state.emit();

    assert(ctx.destroyCalls == 1);
    assert(ctx.siblingCalls == 2);
  }

  // --- State<void>: unbound sibling must not run in same emit ---
  {
    StateNotifyDeleteCtx ctx;
    loka::core::EmitterState state;
    ctx.state = &state;
    ctx.valueState = 0;
    ctx.destroyCalls = 0;
    ctx.siblingCalls = 0;

    state.bind(&StateNotifyHelpers::unbindSibling, &ctx, false);
    state.bind(&StateNotifyHelpers::sibling, &ctx, false);
    state.emit();

    assert(ctx.destroyCalls == 1);
    assert(ctx.siblingCalls == 0);
  }

  // --- State<T>: callback may delete mutable state safely ---
  {
    StateNotifyDeleteCtx ctx;
    ctx.state = 0;
    ctx.valueState = new loka::core::MutableState<int>(0);
    ctx.destroyCalls = 0;
    ctx.siblingCalls = 0;

    ctx.valueState->bind(&StateNotifyHelpers::destroyValueSelf, &ctx, false);
    ctx.valueState->bind(&StateNotifyHelpers::sibling, &ctx, false);
    ctx.valueState->set(1);

    assert(ctx.valueState == 0);
    assert(ctx.destroyCalls == 1);
    assert(ctx.siblingCalls == 0);
  }

  // --- State<T>: unbound sibling must not run in same notify ---
  {
    StateNotifyDeleteCtx ctx;
    loka::core::MutableState<int> valueState(0);
    ctx.state = 0;
    ctx.valueState = &valueState;
    ctx.destroyCalls = 0;
    ctx.siblingCalls = 0;

    valueState.bind(&StateNotifyHelpers::unbindValueSibling, &ctx, false);
    valueState.bind(&StateNotifyHelpers::sibling, &ctx, false);
    valueState.set(1);

    assert(ctx.destroyCalls == 1);
    assert(ctx.siblingCalls == 0);
  }

  printf("==== [testLokaFlowDslV1Core] end ====\n");
}
