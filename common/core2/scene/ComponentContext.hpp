#ifndef DECLARA_CORE2_SCENE_COMPONENTCONTEXT_HPP
#define DECLARA_CORE2_SCENE_COMPONENTCONTEXT_HPP

#include "core2/scene/StateOwner.hpp"

namespace declara
{
  namespace core
  {
    namespace scene
    {
      class Node;
      class IStateOwner;

      class ComponentContext
      {
      public:
        ComponentContext() : parent_(0), owner_(0), stateOwner_(0) {}
        explicit ComponentContext(ComponentContext *parent) : parent_(parent), owner_(0), stateOwner_(0) {}

        ComponentContext *parent() const { return parent_; }
        void setParent(ComponentContext *parent) { parent_ = parent; }
        Node *owner() const { return owner_; }
        void setOwner(Node *owner) { owner_ = owner; }
        IStateOwner *stateOwner() const { return stateOwner_; }
        void setStateOwner(IStateOwner *owner) { stateOwner_ = owner; }

      private:
        friend class ComposableNode;
        ComponentContext *parent_;
        Node *owner_;
        IStateOwner *stateOwner_;
      };

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // DECLARA_CORE2_SCENE_COMPONENTCONTEXT_HPP
