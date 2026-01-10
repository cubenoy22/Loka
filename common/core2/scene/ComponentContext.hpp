#ifndef LOKA_CORE2_SCENE_COMPONENTCONTEXT_HPP
#define LOKA_CORE2_SCENE_COMPONENTCONTEXT_HPP

#include "core2/scene/StateOwner.hpp"
#include "core2/scene/Node.hpp"

class Window;

namespace declara
{
  namespace core
  {
    namespace scene
    {
      class BoundaryNode;
      class Node;
      class IStateOwner;
      class Scene;

      class ComponentContext
      {
      public:
        ComponentContext() : parent_(0), owner_(0), stateOwner_(0), boundary_(0), scene_(0), window_(0), dirtyFlags_(NODE_DIRTY_NONE) {}
        explicit ComponentContext(ComponentContext *parent)
            : parent_(parent), owner_(0), stateOwner_(0), boundary_(0), scene_(0), window_(0), dirtyFlags_(NODE_DIRTY_NONE) {}

        ComponentContext *parent() const { return parent_; }
        void setParent(ComponentContext *parent) { parent_ = parent; }
        Node *owner() const { return owner_; }
        void setOwner(Node *owner) { owner_ = owner; }
        IStateOwner *stateOwner() const { return stateOwner_; }
        void setStateOwner(IStateOwner *owner) { stateOwner_ = owner; }
        BoundaryNode *boundary() const { return boundary_; }
        void setBoundary(BoundaryNode *boundary) { boundary_ = boundary; }
        Scene *scene() const { return scene_; }
        void setScene(Scene *scene) { scene_ = scene; }
        ::Window *window() const { return window_; }
        void setWindow(::Window *window) { window_ = window; }
        NodeDirtyFlags dirtyFlags() const { return dirtyFlags_; }
        void setDirtyFlags(NodeDirtyFlags flags) { dirtyFlags_ = flags; }

      private:
        friend class ComposableNode;
        ComponentContext *parent_;
        Node *owner_;
        IStateOwner *stateOwner_;
        BoundaryNode *boundary_;
        Scene *scene_;
        ::Window *window_;
        NodeDirtyFlags dirtyFlags_;
      };

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // LOKA_CORE2_SCENE_COMPONENTCONTEXT_HPP
