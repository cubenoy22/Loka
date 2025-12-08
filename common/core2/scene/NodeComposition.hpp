#ifndef DECLARA_CORE2_SCENE_NODECOMPOSITION_HPP
#define DECLARA_CORE2_SCENE_NODECOMPOSITION_HPP

#include <vector>
#include <cassert>
#include "core2/scene/Node.hpp"
#include "core2/scene/StreamView.hpp"
#include "core2/scene/node/Conditional.hpp"
#include "core2/scene/ComponentContext.hpp"

namespace declara
{
  namespace core
  {
    namespace scene
    {
      struct NodeComposition
      {
      private:
        // Arena: owns copies of all definitions created during compose
        std::vector<NodeDefinitionBase *> arena_;
        NodeDefinitionBase *root_;
        ComponentContext *context_;
        NodeDefinitionBase *storeBase(const NodeDefinitionBase &def)
        {
          NodeDefinitionBase *cloned = def.clone();
          cloned->setCleanupHook(&NodeComposition::cleanupStoredNode, this);
          arena_.push_back(cloned);
          return cloned;
        }
        static void cleanupStoredNode(NodeDefinitionBase *node, void *context)
        {
          if (!node || !context)
          {
            return;
          }
          NodeComposition *self = static_cast<NodeComposition *>(context);
          self->releaseStoredNode(node);
        }
        void releaseStoredNode(NodeDefinitionBase *node)
        {
          for (size_t i = 0; i < arena_.size(); ++i)
          {
            if (arena_[i] == node)
            {
              arena_[i] = 0;
              break;
            }
          }
        }

        void destroyArena()
        {
          for (size_t i = 0; i < arena_.size(); ++i)
          {
            NodeDefinitionBase *node = arena_[i];
            if (!node)
            {
              continue;
            }
            node->clearCleanupHook();
            delete node;
          }
          arena_.clear();
          root_ = 0;
        }

      public:
        NodeComposition() : root_(0), context_(0) {}

        ~NodeComposition() { this->destroyArena(); }

        // Store a copy of the definition in the arena and return pointer
        template <typename T>
        T *store(const T &def)
        {
          return static_cast<T *>(storeBase(def));
        }
        NodeDefinitionBase *store(const NodeDefinitionBase &def) { return storeBase(def); }

        // Declare root node
        template <typename T>
        T &declare(const T &def)
        {
          T *newRoot = this->store(def);
          this->root_ = newRoot;
          return *newRoot;
        }
        NodeDefinitionBase &declare(const NodeDefinitionBase &def)
        {
          NodeDefinitionBase *newRoot = this->store(def);
          this->root_ = newRoot;
          return *newRoot;
        }
        NodeDefinitionBase &declare(const INestableDefinition &def)
        {
          const NodeDefinitionBase *base = dynamic_cast<const NodeDefinitionBase *>(&def);
          assert(base && "Nestable definitions must derive from NodeDefinitionBase");
          return this->declare(*base);
        }

        void setContext(ComponentContext *context) { context_ = context; }
        ComponentContext *componentContext() const { return context_; }
        bool hasContext() const { return context_ != 0; }

        void reset()
        {
          this->destroyArena();
          context_ = 0;
        }

        template <typename T>
        void useContext(const ContextDefinition<T> &definition, T &value)
        {
          if (!context_)
          {
            return;
          }
          context_->provide(definition, value);
        }

        template <typename T>
        T &requireContext(const ContextDefinition<T> &definition) const
        {
          assert(context_ && "NodeComposition::requireContext requires ComponentContext");
          return context_->require(definition);
        }

        // Create node tree
        Node *createNodeTree() const;

        NodeDefinitionBase *root() const { return root_; }

        // --- DSL helpers ---
        template <typename T>
        declara::core::scene::ConditionalDefinition conditional(const State<bool> &condition, T &x)
        {
          extern T Empty;
          return ConditionalDefinition(ConditionalProps(&condition, &x, &Empty));
        }

        template <typename T>
        declara::core::scene::ConditionalDefinition conditional(State<bool> &condition, T &x)
        {
          extern T Empty;
          return ConditionalDefinition(ConditionalProps(&condition, &x, &Empty));
        }

        template <typename T>
        StreamView<typename std::vector<T>::const_iterator> stream(const std::vector<T> &v)
        {
          return StreamView<typename std::vector<T>::const_iterator>(v.begin(), v.end());
        }

        template <typename It>
        StreamView<It> stream(It begin, It end)
        {
          return StreamView<It>(begin, end);
        }

        template <typename T>
        T *group(const T &x)
        {
          return this->store(x);
        }
        NodeDefinitionBase *group(const INestableDefinition &x)
        {
          const NodeDefinitionBase *base = dynamic_cast<const NodeDefinitionBase *>(&x);
          assert(base && "Nestable definitions must derive from NodeDefinitionBase");
          return this->store(*base);
        }
      };

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // DECLARA_CORE2_SCENE_NODECOMPOSITION_HPP
