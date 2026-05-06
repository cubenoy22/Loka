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
        NodeCompositionSnapshot() : root_(0) {}

        NodeCompositionSnapshot(const NodeCompositionSnapshot &other) : root_(0)
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
          this->clear();
          if (other.root_)
          {
            this->root_ = other.root_->clone();
          }
          return *this;
        }

        ~NodeCompositionSnapshot()
        {
          this->clear();
        }

        void capture(const NodeComposition &composition)
        {
          this->clear();
          if (composition.root())
          {
            this->root_ = composition.root()->clone();
          }
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
    }
  }
}

#endif // LOKA_CORE2_SCENE_COMPOSITION_NODECOMPOSITIONSNAPSHOT_HPP
