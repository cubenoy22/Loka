#ifndef DECLARA_CORE2_SCENE_NODECOMPOSITION_HPP
#define DECLARA_CORE2_SCENE_NODECOMPOSITION_HPP

#include <vector>
#include "core2/scene/Node.hpp"
#include "core2/scene/StreamView.hpp"
#include "core2/scene/node/Conditional.hpp"

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

      public:
        NodeComposition() : root_(0) {}

        ~NodeComposition()
        {
          for (size_t i = 0; i < arena_.size(); ++i)
          {
            delete arena_[i];
          }
        }

        // Store a copy of the definition in the arena and return pointer
        template <typename T>
        T *store(const T &def)
        {
          T *newDef = new T(def);
          arena_.push_back(newDef);
          return newDef;
        }

        // Declare root node
        template <typename T>
        T &declare(const T &def)
        {
          T *newRoot = store(def);
          this->root_ = newRoot;
          return *newRoot;
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
        T &group(T &x) { return x; }
      };

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // DECLARA_CORE2_SCENE_NODECOMPOSITION_HPP
