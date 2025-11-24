#ifndef DECLARA_CORE2_SCENE_NODE_COMPOSABLENODE_HPP
#define DECLARA_CORE2_SCENE_NODE_COMPOSABLENODE_HPP

#include "../Node.hpp"

namespace declara
{
  namespace core
  {
    namespace scene
    {
      // Base class for nodes that build children via compose() when requested
      class ComposableNode : public NestableNode
      {
      public:
        virtual ~ComposableNode()
        {
          clearChildren();
        }
        virtual void compose() = 0;

      protected:
        void clearChildren()
        {
          for (size_t i = 0; i < children_.size(); ++i)
          {
            delete children_[i];
          }
          children_.clear();
        }
      };

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // DECLARA_CORE2_SCENE_NODE_COMPOSABLENODE_HPP
