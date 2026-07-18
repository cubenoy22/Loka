#ifndef LOKA_CORE2_SCENE_NODE_HPP
#define LOKA_CORE2_SCENE_NODE_HPP

#include "core/diag/LifecycleAudit.hpp"

#include <cassert>

// static_assert-like macro. Modern compilers get real checks automatically;
// C++98 release builds keep optional checks lightweight unless explicitly gated.
#if __cplusplus >= 201103L
#define LOKA_STATIC_ASSERT(expr, msg) static_assert((expr), #msg)
#elif defined(NDEBUG)
#define LOKA_STATIC_ASSERT(expr, msg)                                                                                  \
  enum                                                                                                                 \
  {                                                                                                                    \
    static_assert_##msg = 1                                                                                            \
  }
#else
#define LOKA_STATIC_ASSERT(expr, msg) typedef char static_assert_##msg[(expr) ? 1 : -1]
#endif

#include <cstddef>
#include <string>
#include <vector>
#include "dsl/composition/CompositionList.hpp"
#include "core/util/OwnedDef.hpp"

#include "core/State.hpp"
#include "core/Profiler.hpp"
#include "app/scene/ability/CapturableBitmap.hpp"
#include "app/scene/detail/ArenaMath.hpp"

#if defined(TEST_BUILD)
#define TEST_ID(value) testId(value)
#else
#define TEST_ID(value) noop()
#endif

namespace loka
{
  namespace app
  {
    namespace scene
    {
      // NodeDirtyFlags: flags for node dirtiness (C++98-friendly)
      enum NodeDirtyFlags
      {
        NODE_DIRTY_NONE = 0x00,
        NODE_DIRTY_PROPS = 0x01,
        NODE_DIRTY_CHILD = 0x02,
        NODE_DIRTY_LAYOUT = 0x04,
        NODE_DIRTY_MYSELF = 0xFF, // All node-local dirty flags.
        NODE_DIRTY_INITIAL = 0x100
      };

      // ComposeEvent: describes why compose was invoked.
      enum ComposeEvent
      {
        COMPOSE_EVENT_ATTACH = 0,
        COMPOSE_EVENT_UPDATE = 1,
        COMPOSE_EVENT_DETACH = 2
      };

      enum ComposeAttachState
      {
        COMPOSE_ATTACH_STATE_NONE = 0,
        COMPOSE_ATTACH_STATE_PENDING_ATTACH = 1
      };

      struct ComposeAttachLifecycle
      {
        ComposeAttachLifecycle()
            : state(COMPOSE_ATTACH_STATE_NONE)
        {
        }

        void markPendingAttach()
        {
          state = COMPOSE_ATTACH_STATE_PENDING_ATTACH;
        }

        ComposeEvent resolveChildComposeEvent(ComposeEvent parentEvent)
        {
          if (state != COMPOSE_ATTACH_STATE_PENDING_ATTACH)
          {
            return parentEvent;
          }
          state = COMPOSE_ATTACH_STATE_NONE;
          return parentEvent == COMPOSE_EVENT_UPDATE ? COMPOSE_EVENT_ATTACH : parentEvent;
        }

        ComposeAttachState state;
      };

      enum NodeKind
      {
        NODE_KIND_UNKNOWN = 0,
        NODE_KIND_BOX,
        NODE_KIND_ZSTACK,
        NODE_KIND_GRID,
        NODE_KIND_CELL,
        NODE_KIND_COLUMN,
        NODE_KIND_ROW,
        NODE_KIND_TEXT,
        NODE_KIND_BUTTON,
        NODE_KIND_EDIT_TEXT,
        NODE_KIND_POPUP_MENU,
        NODE_KIND_OPEN_FILE_DIALOG,
        NODE_KIND_IMAGE_VIEW,
        NODE_KIND_RECT_SURFACE
      };

      typedef unsigned short NodeTag;
      enum
      {
        NODE_TAG_NONE = 0
      };

      /** The one wish axis a Node may express about its native pair.
          Crosses the wall as observable state alongside the lifecycle facts
          (attached / detached-retained / retired); the native side decides
          policy from facts + hint. No imperative release command exists. */
      enum NativeLifetimeHint
      {
        NATIVE_HINT_DEFAULT = 0,
        NATIVE_HINT_EAGER_RELEASE,
        NATIVE_HINT_DESIRE_STAY
      };

      /** The lifecycle fact: where a node stands relative to the attached
          path. Nodes are born ATTACHED; RETIRED is terminal. Every write
          passes the single door (Node::applyLifecycleFact) whose transition
          table asserts on revival — the lifecycle version of the ABA ban. */
      enum NodeLifecycleFact
      {
        NODE_FACT_ATTACHED = 0,
        NODE_FACT_DETACHED_RETAINED,
        NODE_FACT_RETIRED
      };

      struct INestable;
      class IPlatformController;
      class Node; // forward declaration for NodeContext owner
      struct NodeDefinitionBase;
      struct IProjectedLayoutNode;
      class ComposableNode;
      class BoundaryNode;
      class ComponentContext;
      class IStateOwner;
      struct DirtySourceRegistrar;
      namespace detail
      {
        class NodeArena;
      }

      template <typename NodeT> struct NodeTypeTokenStorage
      {
        static const char value_;
      };

      template <typename NodeT> const char NodeTypeTokenStorage<NodeT>::value_ = 0;

      template <typename NodeT> const void *NodeTypeToken()
      {
        return &NodeTypeTokenStorage<NodeT>::value_;
      }
    } // namespace scene
  } // namespace app

  // Forward declarations for app nodes (for asXxx() methods)
  namespace app
  {
    class BoxNode;
    class ZStackNode;
    class GridNode;
    class CellNode;
    class RowNode;
    class ColumnNode;
    class TextNode;
    class ButtonNode;
    class EditTextNode;
    class PopupMenuNode;
    class OpenFileDialogNode;
    class ImageViewNode;
    class RectSurfaceNode;
  } // namespace app

  namespace app
  {
    namespace scene
    {

      struct LayoutState
      {
        short x;
        short y;
        short width;
        short height;
        short lineHeight;
        short spacing;

        LayoutState()
            : x(0),
              y(0),
              width(0),
              height(0),
              lineHeight(0),
              spacing(0)
        {
        }
      };

      struct IProjectedLayoutNode
      {
        virtual ~IProjectedLayoutNode() {}
        virtual short layoutProjected(IPlatformController *controller, LayoutState &state) = 0;
      };

      bool PrepareProjectedLayout(IPlatformController *controller, Node *node, LayoutState &state);

      // Minimal NodeContext implementation
      struct NodeContext
      {
      public:
        NodeContext()
            : owner_(0),
              deliveredFact_(NODE_FACT_ATTACHED),
              observedLifetimeHint_(NATIVE_HINT_DEFAULT)
        {
        }
        explicit NodeContext(Node *owner)
            : owner_(owner),
              deliveredFact_(NODE_FACT_ATTACHED),
              observedLifetimeHint_(NATIVE_HINT_DEFAULT)
        {
        }
        virtual ~NodeContext() {}

        void setOwner(Node *owner)
        {
          owner_ = owner;
        }
        Node *owner() const
        {
          return owner_;
        }
        /** The consumer-side baseline: the last lifecycle fact this context
            was told about (or read at attach time). Lives inside the wall —
            the one permanent field besides the Node's own fact. */
        NodeLifecycleFact deliveredFact() const
        {
          return deliveredFact_;
        }
        /** The native side's observed copy of the owner Node's lifetime
            hint — the one wish axis, a snapshot never a command. Rides the
            same observation points as the fact: the attach-time read and
            every delivery-walk visit. */
        NativeLifetimeHint lifetimeHint() const
        {
          return observedLifetimeHint_;
        }
        /** The context-side observation point — the one channel. Living
            transitions (A <-> D) arrive from the apply-phase diff walk;
            the terminal RETIRED arrives from the retire door, after
            severing and right before this context is destroyed. */
        virtual void onFactChanged(NodeLifecycleFact previous, NodeLifecycleFact next)
        {
          (void)previous;
          (void)next;
        }
        virtual ICapturableBitmap *asCapturableBitmap()
        {
          return 0;
        }
        virtual const ICapturableBitmap *asCapturableBitmap() const
        {
          return 0;
        }
        virtual void render(IPlatformController *) {}
        virtual short layout(IPlatformController *, LayoutState &)
        {
          return 0;
        }

      private:
        NodeContext(const NodeContext &);
        NodeContext &operator=(const NodeContext &);

        /** Attach-time read (late-subscriber rule): adopt the current
            observable state (fact + hint) as the baseline without firing
            onFactChanged. Defined after Node. */
        void initializeDeliveredFact(Node *ownerNode);
        /** The hint snapshot rides every walk visit (before the same-value
            gate); the fact baseline moves before the hook fires, so
            reentrant reads see the delivered value. Same-value delivery is
            silent. Defined after Node. */
        void deliverFact(Node *ownerNode, NodeLifecycleFact next);

        Node *owner_;
        NodeLifecycleFact deliveredFact_;
        NativeLifetimeHint observedLifetimeHint_;

        friend class Node;
      };

      class Node LOKA_AUDITED(Node)
      {
      public:
        NodeContext *context;
        loka::core::MutableState<NodeDirtyFlags> dirty;
        Node *nextInComposition;
        detail::NodeArena *arenaOwner_;
        // Compose-only signal used to upgrade a child update pass into an
        // attach pass when a parent swaps in a freshly created child.
        // Platform/native code should not depend on this compose-local state
        // for presentation or lifecycle decisions.
        ComposeAttachLifecycle composeAttachLifecycle_;
        std::string testId_;
        NodeTag nodeTag_;
        const void *propsTypeId_;
        NativeLifetimeHint nativeLifetimeHint_;

        Node()
            : context(0),
              dirty(NODE_DIRTY_NONE),
              nextInComposition(0),
              arenaOwner_(0),
              composeAttachLifecycle_(),
              testId_(),
              nodeTag_(NODE_TAG_NONE),
              propsTypeId_(0),
              nativeLifetimeHint_(NATIVE_HINT_DEFAULT),
              lifecycleFact_(NODE_FACT_ATTACHED)
        {
        }

        virtual ~Node()
        {
          // Death is the last retire door: every deletion path funnels here,
          // so a node that skipped the explicit retire/teardown doors (dying
          // with its parent) still turns RETIRED before its context hears
          // the terminal. Already-retired nodes are a silent same-value set.
          this->applyLifecycleFact(NODE_FACT_RETIRED);
          this->releaseContext();
        }

        void setArenaOwner(detail::NodeArena *owner)
        {
          arenaOwner_ = owner;
        }
        detail::NodeArena *arenaOwner() const
        {
          return arenaOwner_;
        }
        bool isArenaAllocated() const
        {
          return arenaOwner_ != 0;
        }
        void markPendingAttachForCompose()
        {
          composeAttachLifecycle_.markPendingAttach();
        }
      protected:
        /** The second observation point (the first is the context's
            onFactChanged): nodes without contexts that own lifecycle
            reactions of their own hear their door writes here. Fires from
            the single door on real transitions only; note that the write in
            ~Node dispatches to the base once a derived destructor has run,
            so destructors must not rely on it. */
        virtual void onLifecycleFactChanged(NodeLifecycleFact previous, NodeLifecycleFact next)
        {
          (void)previous;
          (void)next;
        }

      public:
        /** Owners that park retained branches outside the live composition
            expose them here so every lifecycle walk — fact marking, delivery,
            and context release — descends into them.
            Returns the index-th parked branch root (compacted), 0 past the
            end. The retire door relies on this: a parked branch hands its
            native pair over at the door, never from the reclaim drain. */
        virtual Node *retainedLifecycleBranch(unsigned index)
        {
          (void)index;
          return 0;
        }
        ComposeEvent resolveChildComposeEvent(ComposeEvent parentEvent)
        {
          return composeAttachLifecycle_.resolveChildComposeEvent(parentEvent);
        }

        // Custom operator delete - skip deallocation for arena nodes
        static void operator delete(void *ptr)
        {
          Node *node = static_cast<Node *>(ptr);
          if (node && node->arenaOwner_)
          {
            // Arena handles memory, don't free
            return;
          }
          ::operator delete(ptr);
        }
        virtual void compose() {}
        virtual NodeKind kind() const
        {
          return NODE_KIND_UNKNOWN;
        }
        virtual INestable *asNestable()
        {
          return 0;
        }
        virtual ComposableNode *asComposable()
        {
          return 0;
        }
        virtual BoundaryNode *asBoundary()
        {
          return 0;
        }
        virtual IStateOwner *asStateOwner()
        {
          return 0;
        }
        virtual IProjectedLayoutNode *asProjectedLayoutNode()
        {
          return 0;
        }
        virtual const void *nodeTypeKey() const
        {
          return 0;
        }
        // App node type casts (avoid dynamic_cast for 68k performance)
        virtual ::loka::app::RowNode *asRowNode()
        {
          return 0;
        }
        virtual ::loka::app::ColumnNode *asColumnNode()
        {
          return 0;
        }
        virtual ::loka::app::BoxNode *asBoxNode()
        {
          return 0;
        }
        virtual ::loka::app::ZStackNode *asZStackNode()
        {
          return 0;
        }
        virtual ::loka::app::GridNode *asGridNode()
        {
          return 0;
        }
        virtual ::loka::app::CellNode *asCellNode()
        {
          return 0;
        }
        virtual ::loka::app::TextNode *asTextNode()
        {
          return 0;
        }
        virtual ::loka::app::ButtonNode *asButtonNode()
        {
          return 0;
        }
        virtual ::loka::app::EditTextNode *asEditTextNode()
        {
          return 0;
        }
        virtual ::loka::app::PopupMenuNode *asPopupMenuNode()
        {
          return 0;
        }
        virtual ::loka::app::OpenFileDialogNode *asOpenFileDialogNode()
        {
          return 0;
        }
        virtual ::loka::app::ImageViewNode *asImageViewNode()
        {
          return 0;
        }
        virtual ::loka::app::RectSurfaceNode *asRectSurfaceNode()
        {
          return 0;
        }
        virtual void declareDirtySources(DirtySourceRegistrar &) {}
        // Generic interface query (for findBoundary without RTTI)
        virtual void *queryInterface(const char *name)
        {
          (void)name;
          return 0;
        }
        virtual void render(IPlatformController *controller)
        {
          if (context)
          {
            context->render(controller);
          }
        }
        virtual short layout(IPlatformController *controller, LayoutState &state)
        {
          PROFILE_SECTION("layoutNode");
          if (context)
          {
            return context->layout(controller, state);
          }
          return 0;
        }

        void setContext(NodeContext *ctx)
        {
          if (context == ctx)
          {
            return;
          }
          this->releaseContext();
          context = ctx;
          if (context)
          {
            // Attach-time read (late-subscriber rule): the context adopts
            // the current fact as its baseline; presentation from the
            // current fact is the installing handler's read, not a hook.
            context->setOwner(this);
            context->initializeDeliveredFact(this);
          }
        }

        NodeContext *getContext() const
        {
          return context;
        }
        void setTestId(const std::string &value)
        {
          testId_ = value;
        }
        const std::string &testId() const
        {
          return testId_;
        }
        void setNodeTag(NodeTag tag)
        {
          nodeTag_ = tag;
        }
        NodeTag nodeTag() const
        {
          return nodeTag_;
        }
        void setPropsTypeId(const void *propsTypeId)
        {
          this->propsTypeId_ = propsTypeId;
        }
        const void *propsTypeId() const
        {
          return this->propsTypeId_;
        }
        void setNativeLifetimeHint(NativeLifetimeHint hint)
        {
          nativeLifetimeHint_ = hint;
        }
        NativeLifetimeHint nativeLifetimeHint() const
        {
          return nativeLifetimeHint_;
        }
        NodeLifecycleFact lifecycleFact() const
        {
          return lifecycleFact_;
        }

      private:
        /** Re-projects node-owned child selection during the scheduled walk. */
        virtual void evaluateChildrenForScheduledApply(ComponentContext &, BoundaryNode *)
        {
        }

        /** Lets a retained seat finish branch-closed reconciliation at reentry. */
        virtual bool reconcileForScheduledBranchReentry(ComponentContext &, BoundaryNode *)
        {
          return false;
        }

        /** The single door. Same-value writes are silent (including R->R);
            RETIRED is terminal, so R->A / R->D assert. The three writers are
            the compose door (composeTree ATTACH), the walk door
            (NotifySubtreeNode*), and the retire door (retire/teardown paths)
            — reclaim never writes lifecycle state. */
        void applyLifecycleFact(NodeLifecycleFact next)
        {
          if (lifecycleFact_ == next)
          {
            return;
          }
          if (lifecycleFact_ == NODE_FACT_RETIRED)
          {
            assert(false && "lifecycle fact: RETIRED is terminal; a retired node cannot re-enter the attached path");
            return;
          }
          const NodeLifecycleFact previous = lifecycleFact_;
          lifecycleFact_ = next;
          this->onLifecycleFactChanged(previous, next);
        }
        static void MarkSubtreeLifecycleFact(Node *node, NodeLifecycleFact fact);
        static void DeliverLifecycleFactsSubtree(Node *node);

        NodeLifecycleFact lifecycleFact_;

        friend class BoundaryNode;
        friend class Scene;
        friend void NotifySubtreeNodeDetached(Node *node);
        friend void NotifySubtreeNodeAttached(Node *node);
        // Test backdoor (SceneTestAccess precedent): unit pins drive the
        // delivery walk directly on scene-less node trees.
        friend struct LifecycleFactTestAccess;

        void releaseContext()
        {
          NodeContext *released = context;
          if (!released)
          {
            return;
          }
          context = 0;
          // Terminal delivery: the retire door has already written RETIRED
          // and the context is severed from the node (context == 0), so the
          // observer cannot route back into the tree. One onFactChanged(->R),
          // then the ritual (context destruction). A release while the fact
          // is not RETIRED (context replacement on a live node) is silent —
          // the context's own destructor is its terminal signal.
          released->deliverFact(this, lifecycleFact_);
          delete released;
        }
      };

      struct DirtySourceRegistrar
      {
        virtual ~DirtySourceRegistrar() {}
        // Register a live state as a source of the given dirty flags.
        virtual void markDirtyOnChange(loka::core::StateBase *state, NodeDirtyFlags flags) = 0;
      };

      // --- Generic Props base ---
      struct PropsBase
      {
        virtual ~PropsBase() {}
        typedef Node *(*NodeFactoryFunc)(const PropsBase &);
        virtual NodeFactoryFunc nodeFactory() const = 0;
        virtual bool operator<(const PropsBase &rhs) const = 0;

        // Type ID for RTTI-free type comparison (each Props type gets unique ID)
        virtual const void *propsTypeId() const = 0;
      };

      // --- NodePropsBase (templated common base) ---
      template <class PropsT> struct NodePropsBase : public PropsBase
      {
        static Node *createNode(const PropsBase &base)
        {
          const PropsT &props = static_cast<const PropsT &>(base);
          return new typename PropsT::NodeType(props);
        }
        static NodeFactoryFunc staticFactory()
        {
          return &NodePropsBase::createNode;
        }
        NodeFactoryFunc nodeFactory() const
        {
          return staticFactory();
        }

        // Auto-generate unique TypeID per Props type (address of static local)
        static const void *staticTypeId()
        {
          static char id;
          return &id;
        }
        const void *propsTypeId() const
        {
          return staticTypeId();
        }
      };

      // Forward declaration
      struct INestableDefinition;
      struct IBranchSeatDefinition;
      struct IBranchPolicyScopeDefinition;

      /** Seat-local lifecycle/diff policy values folded from definition-only
          branch annotations. */
      struct BranchPolicies
      {
        BranchPolicies(bool destroy = false, bool deliver = false)
            : destroyOnDetach(destroy),
              deliverWhileDetached(deliver)
        {
        }

        bool destroyOnDetach;
        bool deliverWhileDetached;
      };

      /** Definition-side description of a conditional branch seat. */
      struct IBranchSeatDefinition
      {
        virtual ~IBranchSeatDefinition() {}
        virtual loka::core::State<bool> *branchCondition() const = 0;
        virtual NodeDefinitionBase *branchDefinition(bool condition) const = 0;
        virtual const void *branchSeatTypeId() const = 0;
      };

      /** Definition-only branch-root annotation. It is consumed while the
          boundary builds its seat plan and never enters the runtime tree. */
      struct IBranchPolicyScopeDefinition
      {
        virtual ~IBranchPolicyScopeDefinition() {}
        virtual BranchPolicies branchPolicies() const = 0;
        virtual NodeDefinitionBase *scopedBranchDefinition() const = 0;
      };

      template <typename NodeT, typename PropsT> struct NodePropsApplier
      {
        static bool apply(NodeT *node, const PropsT &props)
        {
          node->props = props;
          return true;
        }
      };

      // Type-erased base for node definitions.
      struct NodeDefinitionBase LOKA_AUDITED(NodeDefinitionBase)
      {
      public:
        typedef void (*CleanupHook)(NodeDefinitionBase *, void *);

        NodeDefinitionBase()
            : cleanupHook_(0),
              cleanupContext_(0),
              nextInComposition(0),
              testId_(),
              hasTestId_(false),
              autoTestId_(false),
              nodeTag_(NODE_TAG_NONE),
              compositionSeatSlot_(-1),
              nativeLifetimeHint_(NATIVE_HINT_DEFAULT)
        {
        }
        NodeDefinitionBase(const NodeDefinitionBase &other)
            : cleanupHook_(0),
              cleanupContext_(0),
              nextInComposition(0),
              testId_(other.testId_),
              hasTestId_(other.hasTestId_),
              autoTestId_(other.autoTestId_),
              nodeTag_(other.nodeTag_),
              compositionSeatSlot_(other.compositionSeatSlot_),
              nativeLifetimeHint_(other.nativeLifetimeHint_)
        {
        }
        NodeDefinitionBase &operator=(const NodeDefinitionBase &other)
        {
          this->cleanupHook_ = 0;
          this->cleanupContext_ = 0;
          this->nextInComposition = 0;
          this->testId_ = other.testId_;
          this->hasTestId_ = other.hasTestId_;
          this->autoTestId_ = other.autoTestId_;
          this->nodeTag_ = other.nodeTag_;
          this->compositionSeatSlot_ = other.compositionSeatSlot_;
          this->nativeLifetimeHint_ = other.nativeLifetimeHint_;
          return *this;
        }
        virtual ~NodeDefinitionBase()
        {
          this->invokeCleanupHook();
        }
        /**
         * Allocation contract:
         * - `clone()` / pointer-returning materialization may return `0` only
         *   for allocation-style failure such as OOM.
         * - contract misuse should be rejected structurally or by debug assert,
         *   not normalized through arbitrary `0` returns.
         */
        virtual Node *create() const = 0;
        virtual Node *createInPlace(void *mem) const = 0;
        virtual size_t nodeSize() const = 0;
        virtual size_t nodeAlign() const = 0;
        virtual NodeDefinitionBase *clone() const = 0;
        virtual NodeKind nodeKind() const = 0;
        virtual const PropsBase *propsBase() const = 0;
        virtual bool hasEquivalentProps(const NodeDefinitionBase &other) const = 0;
        /** Refreshes definition-generation borrows for a retained runtime node
            without applying changed prop values. */
        virtual bool repointRetainedNodeDefinition(Node *node) const
        {
          return node != 0;
        }
        virtual bool applyPropsToNode(Node *node) const = 0;
        virtual bool isCompatibleWithNode(const Node *node) const
        {
          return node && node->kind() == this->nodeKind();
        }
        virtual bool isBoundary() const
        {
          return false;
        }
        virtual INestableDefinition *asNestableDefinition()
        {
          return 0;
        }
        virtual IBranchSeatDefinition *asBranchSeatDefinition()
        {
          return 0;
        }
        virtual IBranchPolicyScopeDefinition *asBranchPolicyScopeDefinition()
        {
          return 0;
        }
        virtual NodeDefinitionBase *retainedDefinitionBranch(unsigned index)
        {
          (void)index;
          return 0;
        }
        NodeDefinitionBase *nextInComposition;

        void setCleanupHook(CleanupHook hook, void *context)
        {
          this->cleanupHook_ = hook;
          this->cleanupContext_ = context;
        }
        void clearCleanupHook()
        {
          this->cleanupHook_ = 0;
          this->cleanupContext_ = 0;
        }
        void setTestId(const char *value)
        {
          if (value && value[0] != '\0')
          {
            this->testId_ = value;
            this->hasTestId_ = true;
            this->autoTestId_ = false;
            return;
          }
          this->testId_.clear();
          this->hasTestId_ = false;
          this->autoTestId_ = false;
        }
        void setAutoTestId()
        {
          this->testId_.clear();
          this->hasTestId_ = false;
          this->autoTestId_ = true;
        }
        bool hasTestId() const
        {
          return this->hasTestId_;
        }
        bool wantsAutoTestId() const
        {
          return this->autoTestId_;
        }
        const std::string &testIdValue() const
        {
          return this->testId_;
        }
        void setNodeTag(NodeTag tag)
        {
          this->nodeTag_ = tag;
        }
        NodeTag nodeTag() const
        {
          return this->nodeTag_;
        }
        void setCompositionSeatSlot(int slot)
        {
          this->compositionSeatSlot_ = slot;
        }
        int compositionSeatSlot() const
        {
          return this->compositionSeatSlot_;
        }
        void setNativeLifetimeHint(NativeLifetimeHint hint)
        {
          this->nativeLifetimeHint_ = hint;
        }
        NativeLifetimeHint nativeLifetimeHint() const
        {
          return this->nativeLifetimeHint_;
        }
        void copyTestIdPolicyFrom(const NodeDefinitionBase &other)
        {
          this->testId_ = other.testId_;
          this->hasTestId_ = other.hasTestId_;
          this->autoTestId_ = other.autoTestId_;
          this->nodeTag_ = other.nodeTag_;
          this->compositionSeatSlot_ = other.compositionSeatSlot_;
          this->nativeLifetimeHint_ = other.nativeLifetimeHint_;
        }

      protected:
        void invokeCleanupHook()
        {
          if (!this->cleanupHook_)
          {
            return;
          }
          CleanupHook hook = this->cleanupHook_;
          void *context = this->cleanupContext_;
          this->cleanupHook_ = 0;
          this->cleanupContext_ = 0;
          hook(this, context);
        }

      private:
        CleanupHook cleanupHook_;
        void *cleanupContext_;
        std::string testId_;
        bool hasTestId_;
        bool autoTestId_;
        NodeTag nodeTag_;
        int compositionSeatSlot_;
        NativeLifetimeHint nativeLifetimeHint_;
      };

      // Shared DSL mixin for attaching test ids to node definitions.
      template <class DerivedT> struct TestIdDslMixin
      {
        DerivedT &noop()
        {
          return *static_cast<DerivedT *>(this);
        }
        DerivedT &testId(const char *value)
        {
          DerivedT *self = static_cast<DerivedT *>(this);
          self->setTestId(value);
          return *self;
        }
        DerivedT &testId()
        {
          DerivedT *self = static_cast<DerivedT *>(this);
          self->setAutoTestId();
          return *self;
        }
      };

      template <class PropsT, class NodeT> struct NodeDefinition : public NodeDefinitionBase
      {
        typedef PropsT PropsType;
        typedef NodeT NodeType;

        // Optional static check when PropsT/NodeT have TypeTag (via SFINAE)
#if __cplusplus >= 201103L
        LOKA_STATIC_ASSERT((typename PropsT::TypeTag *)0 == (typename NodeT::TypeTag *)0, props_node_type_mismatch);
#elif defined(LOKA_NODEDEF_CHECK_TYPETAG)
        LOKA_STATIC_ASSERT((typename PropsT::TypeTag *)0 == (typename NodeT::TypeTag *)0, props_node_type_mismatch);
#endif
        PropsT props;
        NodeDefinition()
            : props()
        {
        }
        NodeDefinition(const PropsT &p)
            : props(p)
        {
        }
        NodeDefinition &set(const PropsT &p)
        {
          props = p;
          return *this;
        }
        NodeDefinition &tag(NodeTag value)
        {
          this->setNodeTag(value);
          return *this;
        }
        NodeDefinition &lifetimeHint(NativeLifetimeHint value)
        {
          this->setNativeLifetimeHint(value);
          return *this;
        }
        template <typename F> NodeDefinition &mutate(F f)
        {
          f(props);
          return *this;
        }
        Node *create() const
        {
          Node *node = new NodeT(props);
          if (node)
          {
            node->setPropsTypeId(PropsT::staticTypeId());
            node->setNodeTag(this->nodeTag());
            node->setNativeLifetimeHint(this->nativeLifetimeHint());
          }
          return node;
        }
        Node *createInPlace(void *mem) const
        {
          Node *node = new (mem) NodeT(props);
          if (node)
          {
            node->setPropsTypeId(PropsT::staticTypeId());
            node->setNodeTag(this->nodeTag());
            node->setNativeLifetimeHint(this->nativeLifetimeHint());
          }
          return node;
        }
        size_t nodeSize() const
        {
          return sizeof(NodeT);
        }
        size_t nodeAlign() const
        {
          return detail::AlignOf<NodeT>::value;
        }
        virtual NodeDefinitionBase *clone() const
        {
          return new NodeDefinition(*this);
        }
        virtual bool isCompatibleWithNode(const Node *node) const
        {
          return node && node->propsTypeId() == PropsT::staticTypeId();
        }
        virtual NodeKind nodeKind() const
        {
          NodeT probe(props);
          return probe.kind();
        }
        virtual const PropsBase *propsBase() const
        {
          return &props;
        }
        virtual bool hasEquivalentProps(const NodeDefinitionBase &other) const
        {
          const PropsBase *otherProps = other.propsBase();
          if (!otherProps)
          {
            return false;
          }
          if (this->props.propsTypeId() != otherProps->propsTypeId())
          {
            return false;
          }
          return !(this->props < *otherProps) && !(*otherProps < this->props);
        }
        virtual bool applyPropsToNode(Node *node) const
        {
          if (!this->isCompatibleWithNode(node))
          {
            return false;
          }
          NodeT *typed = static_cast<NodeT *>(node);
          bool applied = NodePropsApplier<NodeT, PropsT>::apply(typed, this->props);
          if (applied)
          {
            typed->setNodeTag(this->nodeTag());
            typed->setNativeLifetimeHint(this->nativeLifetimeHint());
          }
          return applied;
        }
      };

      // --- Interface for nestable NodeDefinition ---
      struct INestableDefinition
      {
        virtual ~INestableDefinition() {}
        virtual void addChild(NodeDefinitionBase *child) = 0;
        virtual void addOwnedChild(NodeDefinitionBase *child)
        {
          addChild(child);
        }
        virtual NodeDefinitionBase *childrenHead() const = 0;
        virtual size_t childrenCount() const = 0;

        // Type cast (avoid dynamic_cast for 68k performance)
        virtual const NodeDefinitionBase *asNodeDefinitionBase() const = 0;

        // Overloads
        INestableDefinition &operator<<(NodeDefinitionBase &child);
        INestableDefinition &operator<<(const NodeDefinitionBase &child);
        INestableDefinition &operator<<(NodeDefinitionBase *ownedChild);

        // Explicit single element operator<< (no begin/end)
        // Type-safe: accept only NodeDefinitionBase

        // vector<NodeDefinitionBase*> explicit overload (C++98)
        INestableDefinition &operator<<(const std::vector<NodeDefinitionBase *> &container);

        // For future extensibility
      };

      template <class DerivedT> struct NestableDslMixin
      {
        DerivedT &operator<<(NodeDefinitionBase &child)
        {
          DerivedT *self = static_cast<DerivedT *>(this);
          static_cast<INestableDefinition &>(*self) << child;
          return *self;
        }

        DerivedT &operator<<(const NodeDefinitionBase &child)
        {
          DerivedT *self = static_cast<DerivedT *>(this);
          static_cast<INestableDefinition &>(*self) << child;
          return *self;
        }

        DerivedT &operator<<(NodeDefinitionBase *ownedChild)
        {
          DerivedT *self = static_cast<DerivedT *>(this);
          static_cast<INestableDefinition &>(*self) << ownedChild;
          return *self;
        }

        DerivedT &operator<<(const std::vector<NodeDefinitionBase *> &container)
        {
          DerivedT *self = static_cast<DerivedT *>(this);
          static_cast<INestableDefinition &>(*self) << container;
          return *self;
        }
      };

      // --- Helper base class for nestable definitions owning children ---
      class NestableDefinitionBase : public INestableDefinition
      {
      public:
        NestableDefinitionBase()
            : children_()
        {
        }
        NestableDefinitionBase(const NestableDefinitionBase &other)
            : children_()
        {
          this->replaceChildrenFrom(other);
        }
        NestableDefinitionBase &operator=(const NestableDefinitionBase &other)
        {
          if (this != &other)
          {
            this->replaceChildrenFrom(other);
          }
          return *this;
        }
        virtual ~NestableDefinitionBase()
        {
          this->clearChildren();
        }

        virtual void addChild(NodeDefinitionBase *child)
        {
          if (!child)
          {
            return;
          }
          children_.appendClone(*child);
        }

        virtual void addOwnedChild(NodeDefinitionBase *child)
        {
          if (!child)
          {
            return;
          }
          children_.appendOwned(child);
        }

        virtual NodeDefinitionBase *childrenHead() const
        {
          return children_.head();
        }
        virtual size_t childrenCount() const
        {
          return children_.count();
        }

      protected:
        void clearChildren()
        {
          children_.clear();
        }

        template <typename BaseT>
        bool assignNestableBase(BaseT &baseSelf, const BaseT &otherBase, const NestableDefinitionBase &otherChildren)
        {
          if (!replaceChildrenFrom(otherChildren))
          {
            return false;
          }
          baseSelf = otherBase;
          return true;
        }

        bool replaceChildrenFrom(const NestableDefinitionBase &other)
        {
          loka::dsl::CompositionList<NodeDefinitionBase> newChildren;
          NodeDefinitionBase *cur = other.children_.head();
          while (cur)
          {
            loka::core::OwnedDef<NodeDefinitionBase> child(cur ? cur->clone() : 0);
            if (!child.isSet())
            {
              return false;
            }
            newChildren.appendOwned(child.take());
            cur = cur->nextInComposition;
          }
          clearChildren();
          newChildren.detachTo(children_);
          return true;
        }

      private:
        loka::dsl::CompositionList<NodeDefinitionBase> children_;
      };

      template <class PropsT, class NodeT, class DerivedT>
      struct NestableNodeDefinition : public NodeDefinition<PropsT, NodeT>,
                                      public NestableDefinitionBase,
                                      public NestableDslMixin<DerivedT>
      {
        typedef NodeDefinition<PropsT, NodeT> BaseType;
        using NestableDslMixin<DerivedT>::operator<<;

        NestableNodeDefinition()
            : BaseType(),
              NestableDefinitionBase()
        {
        }

        explicit NestableNodeDefinition(const PropsT &p)
            : BaseType(p),
              NestableDefinitionBase()
        {
        }

        NestableNodeDefinition(const NestableNodeDefinition &other)
            : BaseType(),
              NestableDefinitionBase()
        {
          this->assignFromDefinition(other);
        }

        NestableNodeDefinition &operator=(const NestableNodeDefinition &other)
        {
          if (this != &other)
          {
            this->assignFromDefinition(other);
          }
          return *this;
        }

        virtual INestableDefinition *asNestableDefinition()
        {
          return static_cast<DerivedT *>(this);
        }

        virtual const NodeDefinitionBase *asNodeDefinitionBase() const
        {
          return static_cast<const DerivedT *>(this);
        }

        virtual NodeDefinitionBase *clone() const
        {
          DerivedT *copy = new DerivedT();
          if (!copy)
          {
            return 0;
          }
          if (!copy->assignFromDefinition(*this))
          {
            delete copy;
            return 0;
          }
          return copy;
        }

      protected:
        bool assignFromDefinition(const NestableNodeDefinition &other)
        {
          return this->assignNestableBase(
              static_cast<BaseType &>(*this), static_cast<const BaseType &>(other), other);
        }
      };

      // --- Interface for nestable Node/Definition ---
      struct INestable
      {
        virtual ~INestable() {}
        virtual void addChild(Node *child) = 0;
        virtual bool replaceChild(Node *oldChild, Node *newChild) = 0;
        virtual void detachChildrenTo(std::vector<Node *> &out) = 0;
        virtual Node *childrenHead() const = 0;
        virtual size_t childrenCount() const = 0;
      };

      // --- Helper node which owns children ---
      class NestableNode : public Node, public INestable
      {
      public:
        NestableNode()
            : Node(),
              children_()
        {
        }
        virtual ~NestableNode()
        {
          clearChildrenInternal(false);
        }

        virtual void addChild(Node *child)
        {
          if (child)
          {
            children_.appendOwned(child);
          }
        }

        virtual bool replaceChild(Node *oldChild, Node *newChild)
        {
          return children_.replace(oldChild, newChild);
        }

        virtual void detachChildrenTo(std::vector<Node *> &out)
        {
          children_.detachTo(out);
        }

        virtual Node *childrenHead() const
        {
          return children_.head();
        }
        virtual size_t childrenCount() const
        {
          return children_.count();
        }
        virtual INestable *asNestable()
        {
          return this;
        }

      protected:
        void clearChildrenInternal(bool deleteArenaChildren)
        {
          if (children_.count() == 0)
          {
            return;
          }
          std::vector<Node *> nodes;
          children_.detachTo(nodes);
          for (size_t i = 0; i < nodes.size(); ++i)
          {
            Node *child = nodes[i];
            if (!child)
            {
              continue;
            }
            if (!deleteArenaChildren && child->isArenaAllocated())
            {
              continue;
            }
            delete child;
          }
        }
        loka::dsl::CompositionList<Node> children_;
      };

      inline void NodeContext::initializeDeliveredFact(Node *ownerNode)
      {
        if (!ownerNode)
        {
          return;
        }
        deliveredFact_ = ownerNode->lifecycleFact();
        observedLifetimeHint_ = ownerNode->nativeLifetimeHint();
      }

      inline void NodeContext::deliverFact(Node *ownerNode, NodeLifecycleFact next)
      {
        if (ownerNode)
        {
          observedLifetimeHint_ = ownerNode->nativeLifetimeHint();
        }
        if (deliveredFact_ == next)
        {
          return;
        }
        const NodeLifecycleFact previous = deliveredFact_;
        deliveredFact_ = next;
        this->onFactChanged(previous, next);
      }

      /** Marks a subtree's lifecycle fact through the single door, without
          delivering anything. Retained branches parked outside the live
          composition (Conditional slots) are not reached — they keep their
          own fact until their owner's door writes it. */
      inline void Node::MarkSubtreeLifecycleFact(Node *node, NodeLifecycleFact fact)
      {
        if (!node)
        {
          return;
        }
        node->applyLifecycleFact(fact);
        for (unsigned i = 0; Node *branch = node->retainedLifecycleBranch(i); ++i)
        {
          MarkSubtreeLifecycleFact(branch, fact);
        }
        INestable *nestable = node->asNestable();
        if (!nestable)
        {
          return;
        }
        for (Node *child = nestable->childrenHead(); child; child = child->nextInComposition)
        {
          MarkSubtreeLifecycleFact(child, fact);
        }
      }

      /** The apply-phase delivery walk: hands each context the diff between
          its baseline (deliveredFact_) and the node's fact — the only path
          that turns living transitions (A <-> D) into onFactChanged calls.
          Descends into parked retained branches through the owner's virtual,
          so a branch swapped out of the composition still hears its detach
          at the next apply. Reclaim never runs this walk. */
      inline void Node::DeliverLifecycleFactsSubtree(Node *node)
      {
        if (!node)
        {
          return;
        }
        if (node->context)
        {
          node->context->deliverFact(node, node->lifecycleFact_);
        }
        for (unsigned i = 0; Node *branch = node->retainedLifecycleBranch(i); ++i)
        {
          DeliverLifecycleFactsSubtree(branch);
        }
        INestable *nestable = node->asNestable();
        if (!nestable)
        {
          return;
        }
        for (Node *child = nestable->childrenHead(); child; child = child->nextInComposition)
        {
          DeliverLifecycleFactsSubtree(child);
        }
      }

      /** Walk door, detach direction: writes the retained-detach fact down a
          subtree. Contexts stay alive and hear the change from the
          apply-phase delivery walk — never synchronously from here. Nodes
          that own retained branches (Conditional) read their own fact to
          stop announcing swaps while an ancestor keeps them off the
          attached path. */
      inline void NotifySubtreeNodeDetached(Node *node)
      {
        if (!node)
        {
          return;
        }
        node->applyLifecycleFact(NODE_FACT_DETACHED_RETAINED);
        INestable *nestable = node->asNestable();
        if (!nestable)
        {
          return;
        }
        for (Node *child = nestable->childrenHead(); child; child = child->nextInComposition)
        {
          NotifySubtreeNodeDetached(child);
        }
      }

      /** Walk door, re-entry direction: first entry is announced by
          setContext()'s attach-time read. The walk follows live children
          only, so branches swapped out while hidden stay hidden and the
          current active path is shown at the next apply. */
      inline void NotifySubtreeNodeAttached(Node *node)
      {
        if (!node)
        {
          return;
        }
        node->applyLifecycleFact(NODE_FACT_ATTACHED);
        INestable *nestable = node->asNestable();
        if (!nestable)
        {
          return;
        }
        for (Node *child = nestable->childrenHead(); child; child = child->nextInComposition)
        {
          NotifySubtreeNodeAttached(child);
        }
      }

      inline INestableDefinition &INestableDefinition::operator<<(NodeDefinitionBase &child)
      {
        addChild(&child);
        return *this;
      }
      inline INestableDefinition &INestableDefinition::operator<<(const NodeDefinitionBase &child)
      {
        addChild(const_cast<NodeDefinitionBase *>(&child));
        return *this;
      }

      // operator<< implementation for generic containers (e.g., vector)
      inline INestableDefinition &INestableDefinition::operator<<(const std::vector<NodeDefinitionBase *> &container)
      {
        for (size_t i = 0; i < container.size(); ++i)
        {
          addChild(container[i]);
        }
        return *this;
      }

      inline INestableDefinition &INestableDefinition::operator<<(NodeDefinitionBase *ownedChild)
      {
        addOwnedChild(ownedChild);
        return *this;
      }
    } // namespace scene
  } // namespace app
} // namespace loka

// Conditional node inline implementations removed from this header to reduce coupling

#endif // LOKA_CORE2_SCENE_NODE_HPP
