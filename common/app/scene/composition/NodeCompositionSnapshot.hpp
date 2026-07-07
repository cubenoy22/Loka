#ifndef LOKA_CORE2_SCENE_COMPOSITION_NODECOMPOSITIONSNAPSHOT_HPP
#define LOKA_CORE2_SCENE_COMPOSITION_NODECOMPOSITIONSNAPSHOT_HPP

#include "app/scene/composition/NodeComposition.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      struct NodeCompositionSnapshot
      {
      private:
        bool replaceWithClone(const NodeDefinitionBase *root)
        {
          if (!root)
          {
            this->clear();
            return true;
          }
          NodeDefinitionBase *nextRoot = root->clone();
          if (!nextRoot)
          {
            return false;
          }
          this->clear();
          this->root_ = nextRoot;
          return true;
        }

      public:
        NodeCompositionSnapshot()
            : root_(0)
        {
        }

        NodeCompositionSnapshot(const NodeCompositionSnapshot &other)
            : root_(0)
        {
          if (other.root_)
          {
            this->root_ = other.root_->clone();
          }
        }

        NodeCompositionSnapshot &operator=(const NodeCompositionSnapshot &other)
        {
          if (this == &other)
          {
            return *this;
          }
          this->replaceWithClone(other.root_);
          return *this;
        }

        ~NodeCompositionSnapshot()
        {
          this->clear();
        }

        void capture(const NodeComposition &composition)
        {
          this->replaceWithClone(composition.root());
        }

        void clear()
        {
          if (this->root_)
          {
            delete this->root_;
            this->root_ = 0;
          }
        }

        bool empty() const
        {
          return this->root_ == 0;
        }

        NodeDefinitionBase *root() const
        {
          return this->root_;
        }

      private:
        NodeDefinitionBase *root_;
      };
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_COMPOSITION_NODECOMPOSITIONSNAPSHOT_HPP
