#ifndef LOKA_CORE2_SCENE_NODE_HPP
#define LOKA_CORE2_SCENE_NODE_HPP

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

#include "core/State.hpp"
#include "core/Profiler.hpp"
#include "app/scene/ability/CapturableBitmap.hpp"

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

      struct INestable;
      class IPlatformController;
      class Node; // forward declaration for NodeContext owner
      struct IProjectedLayoutNode;
      class ComposableNode;
      class BoundaryNode;
      class IStateOwner;
      struct DirtySourceRegistrar;

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
            : owner_(0)
        {
        }
        explicit NodeContext(Node *owner)
            : owner_(owner)
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
        virtual ICapturableBitmap *asCapturableBitmap()
        {
          return 0;
        }
        virtual const ICapturableBitmap *asCapturableBitmap() const
        {
          return 0;
        }
        virtual void onNodeAttached() {}
        virtual void onNodeDetached() {}
        virtual void render(IPlatformController *) {}
        virtual short layout(IPlatformController *, LayoutState &)
        {
          return 0;
        }

      private:
        NodeContext(const NodeContext &);
        NodeContext &operator=(const NodeContext &);

        Node *owner_;
      };

      class Node
      {
      public:
        NodeContext *context;
        loka::core::MutableState<NodeDirtyFlags> dirty;
        Node *nextInComposition;
        bool arenaAllocated_;
        // Compose-only signal used to upgrade a child update pass into an
        // attach pass when a parent swaps in a freshly created child.
        // Platform/native code should not depend on this compose-local state
        // for presentation or lifecycle decisions.
        ComposeAttachLifecycle composeAttachLifecycle_;
        std::string testId_;
        NodeTag nodeTag_;

        Node()
            : context(0),
              dirty(NODE_DIRTY_NONE),
              nextInComposition(0),
              arenaAllocated_(false),
              composeAttachLifecycle_(),
              testId_(),
              nodeTag_(NODE_TAG_NONE)
        {
        }

        virtual ~Node()
        {
          if (context)
          {
            delete context;
            context = 0;
          }
        }

        void setArenaAllocated(bool v)
        {
          arenaAllocated_ = v;
        }
        bool isArenaAllocated() const
        {
          return arenaAllocated_;
        }
        void markPendingAttachForCompose()
        {
          composeAttachLifecycle_.markPendingAttach();
        }
        ComposeEvent resolveChildComposeEvent(ComposeEvent parentEvent)
        {
          return composeAttachLifecycle_.resolveChildComposeEvent(parentEvent);
        }

        // Custom operator delete - skip deallocation for arena nodes
        static void operator delete(void *ptr)
        {
          Node *node = static_cast<Node *>(ptr);
          if (node && node->arenaAllocated_)
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
          if (context)
          {
            delete context;
          }
          context = ctx;
          if (context)
          {
            context->setOwner(this);
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

      template <typename NodeT, typename PropsT> struct NodePropsApplier
      {
        static bool apply(NodeT *node, const PropsT &props)
        {
          node->props = props;
          return true;
        }
      };

      // C++98 alignof alternative
      template <typename T> struct AlignOfHelper
      {
        char c;
        T t;
      };
      template <typename T> struct AlignOf
      {
        enum
        {
          value = sizeof(AlignOfHelper<T>) - sizeof(T)
        };
      };

      // Type-erased base for node definitions.
      struct NodeDefinitionBase
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
              nodeTag_(NODE_TAG_NONE)
        {
        }
        NodeDefinitionBase(const NodeDefinitionBase &other)
            : cleanupHook_(0),
              cleanupContext_(0),
              nextInComposition(0),
              testId_(other.testId_),
              hasTestId_(other.hasTestId_),
              autoTestId_(other.autoTestId_),
              nodeTag_(other.nodeTag_)
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
          return *this;
        }
        virtual ~NodeDefinitionBase()
        {
          this->invokeCleanupHook();
        }
        virtual Node *create() const = 0;
        virtual Node *createInPlace(void *mem) const = 0;
        virtual size_t nodeSize() const = 0;
        virtual size_t nodeAlign() const = 0;
        virtual NodeDefinitionBase *clone() const = 0;
        virtual NodeKind nodeKind() const = 0;
        virtual const PropsBase *propsBase() const = 0;
        virtual bool hasEquivalentProps(const NodeDefinitionBase &other) const = 0;
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
        void copyTestIdPolicyFrom(const NodeDefinitionBase &other)
        {
          this->testId_ = other.testId_;
          this->hasTestId_ = other.hasTestId_;
          this->autoTestId_ = other.autoTestId_;
          this->nodeTag_ = other.nodeTag_;
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
            node->setNodeTag(this->nodeTag());
          }
          return node;
        }
        Node *createInPlace(void *mem) const
        {
          Node *node = new (mem) NodeT(props);
          if (node)
          {
            node->setNodeTag(this->nodeTag());
          }
          return node;
        }
        size_t nodeSize() const
        {
          return sizeof(NodeT);
        }
        size_t nodeAlign() const
        {
          return AlignOf<NodeT>::value;
        }
        virtual NodeDefinitionBase *clone() const
        {
          return new NodeDefinition(*this);
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
          this->copyChildrenFrom(other);
        }
        NestableDefinitionBase &operator=(const NestableDefinitionBase &other)
        {
          if (this != &other)
          {
            this->copyChildrenFrom(other);
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

        void copyChildrenFrom(const NestableDefinitionBase &other)
        {
          std::vector<NodeDefinitionBase *> newChildren;
          NodeDefinitionBase *cur = other.children_.head();
#if defined(__EXCEPTIONS)
          try
          {
            while (cur)
            {
              NodeDefinitionBase *child = cur ? cur->clone() : 0;
              newChildren.push_back(child);
              cur = cur->nextInComposition;
            }
          }
          catch (...)
          {
            for (size_t i = 0; i < newChildren.size(); ++i)
            {
              delete newChildren[i];
            }
            throw;
          }
#else
          while (cur)
          {
            NodeDefinitionBase *child = cur ? cur->clone() : 0;
            newChildren.push_back(child);
            cur = cur->nextInComposition;
          }
#endif
          clearChildren();
          for (size_t i = 0; i < newChildren.size(); ++i)
          {
            children_.appendOwned(newChildren[i]);
          }
        }

      private:
        loka::dsl::CompositionList<NodeDefinitionBase> children_;
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
