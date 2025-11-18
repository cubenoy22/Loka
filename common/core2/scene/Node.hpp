#ifndef DECLARA_CORE2_SCENE_NODE_HPP
#define DECLARA_CORE2_SCENE_NODE_HPP

// static_assert-like macro for C++98
#define STATIC_ASSERT(expr, msg) typedef char static_assert_##msg[(expr) ? 1 : -1]

#include <cstddef>
#include <vector>

#include "../../core/State.hpp"
// StreamView is only needed by NodeComposition; avoid including here to reduce coupling

namespace declara
{
  namespace core
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
        NODE_DIRTY_MYSELF = 0xFF // 全dirty
      };

      struct NodeContext; // Opaque type

      // Minimal NodeContext implementation
      struct NodeContext
      {
        // TODO: Add actual context data
        virtual ~NodeContext() {}
      };

      class Node
      {
      public:
        NodeContext *context;
        MutableState<NodeDirtyFlags> dirty;
        virtual ~Node() {}
        virtual void compose() {}

        Node() : context(0), dirty(NODE_DIRTY_NONE) {}
      };

      // --- Generic Props base ---
      struct PropsBase
      {
        virtual ~PropsBase() {}
        typedef Node *(*NodeFactoryFunc)(const PropsBase &);
        virtual NodeFactoryFunc nodeFactory() const = 0;
        virtual bool operator<(const PropsBase &rhs) const = 0;
      };

      // --- NodePropsBase (templated common base) ---
      template <class PropsT>
      struct NodePropsBase : public PropsBase
      {
        static Node *createNode(const PropsBase &base)
        {
          const PropsT &props = static_cast<const PropsT &>(base);
          return new typename PropsT::NodeType(props);
        }
        static NodeFactoryFunc staticFactory() { return &NodePropsBase::createNode; }
        NodeFactoryFunc nodeFactory() const { return staticFactory(); }
      };

      // --- NodeDefinitionの型消去基底 ---
      struct NodeDefinitionBase
      {
      public:
        typedef void (*CleanupHook)(NodeDefinitionBase *, void *);

        NodeDefinitionBase() : cleanupHook_(0), cleanupContext_(0) {}
        NodeDefinitionBase(const NodeDefinitionBase &) : cleanupHook_(0), cleanupContext_(0) {}
        NodeDefinitionBase &operator=(const NodeDefinitionBase &)
        {
          this->cleanupHook_ = 0;
          this->cleanupContext_ = 0;
          return *this;
        }
        virtual ~NodeDefinitionBase() { this->invokeCleanupHook(); }
        virtual Node *create() const = 0;
        virtual NodeDefinitionBase *clone() const = 0;

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
      };

      // --- NodeDefinition: Props/Nodeの外部ラッパー（Propsをメンバーとして持つインスタンス型） ---
      template <class PropsT, class NodeT>
      struct NodeDefinition : public NodeDefinitionBase
      {
        typedef PropsT PropsType;
        typedef NodeT NodeType;

        // Optional static check when PropsT/NodeT have TypeTag (via SFINAE)
#ifdef DECLARA_NODEDEF_CHECK_TYPETAG
        STATIC_ASSERT((typename PropsT::TypeTag *)0 == (typename NodeT::TypeTag *)0, props_node_type_mismatch);
#endif
        PropsT props;
        NodeDefinition() : props() {}
        NodeDefinition(const PropsT &p) : props(p) {}
        NodeDefinition &set(const PropsT &p)
        {
          props = p;
          return *this;
        }
        template <typename F>
        NodeDefinition &mutate(F f)
        {
          f(props);
          return *this;
        }
        Node *create() const { return new NodeT(props); }
        virtual NodeDefinitionBase *clone() const { return new NodeDefinition(*this); }
      };

      // --- Interface for nestable NodeDefinition ---
      struct INestableDefinition
      {
        virtual ~INestableDefinition() {}
        virtual void addChild(NodeDefinitionBase *child) = 0;
        virtual void addOwnedChild(NodeDefinitionBase *child) { addChild(child); }
        virtual const std::vector<NodeDefinitionBase *> &getChildren() const = 0;

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

      // --- Helper base class for nestable definitions owning children ---
      class NestableDefinitionBase : public INestableDefinition
      {
      public:
        NestableDefinitionBase() : children_() {}
        NestableDefinitionBase(const NestableDefinitionBase &other) : children_()
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
          children_.push_back(child->clone());
        }

        virtual void addOwnedChild(NodeDefinitionBase *child)
        {
          if (!child)
          {
            return;
          }
          children_.push_back(child);
        }

        virtual const std::vector<NodeDefinitionBase *> &getChildren() const
        {
          return children_;
        }

      protected:
        void clearChildren()
        {
          for (size_t i = 0; i < children_.size(); ++i)
          {
            delete children_[i];
          }
          children_.clear();
        }

        void copyChildrenFrom(const NestableDefinitionBase &other)
        {
          std::vector<NodeDefinitionBase *> newChildren;
          const std::vector<NodeDefinitionBase *> &otherChildren = other.children_;
          try
          {
            for (size_t i = 0; i < otherChildren.size(); ++i)
            {
              NodeDefinitionBase *child = otherChildren[i] ? otherChildren[i]->clone() : 0;
              newChildren.push_back(child);
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
          clearChildren();
          children_ = newChildren;
        }

      private:
        std::vector<NodeDefinitionBase *> children_;
      };

      // --- Interface for nestable Node/Definition ---
      struct INestable
      {
        virtual ~INestable() {}
        virtual void addChild(Node *child) = 0;
        virtual const std::vector<Node *> &getChildren() const = 0;
      };

      // --- Helper node which owns children ---
      class NestableNode : public Node, public INestable
      {
      public:
        NestableNode() : Node() {}
        virtual ~NestableNode()
        {
          for (size_t i = 0; i < children_.size(); ++i)
          {
            delete children_[i];
          }
        }

        virtual void addChild(Node *child)
        {
          if (child)
          {
            children_.push_back(child);
          }
        }

        virtual const std::vector<Node *> &getChildren() const { return children_; }

      protected:
        std::vector<Node *> children_;
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
  } // namespace core
} // namespace declara

// Conditional node inline implementations removed from this header to reduce coupling

#endif // DECLARA_CORE2_SCENE_NODE_HPP
