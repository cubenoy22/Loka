#ifndef LOKA_CORE2_SCENE_CONTEXT_COMPONENTCONTEXT_HPP
#define LOKA_CORE2_SCENE_CONTEXT_COMPONENTCONTEXT_HPP

#include "app/scene/state/StateOwner.hpp"
#include "app/scene/Node.hpp"

class Window;

namespace loka
{
  namespace app
  {
    namespace scene
    {
      class BoundaryNode;
      class Node;
      class IStateOwner;
      class IPlatformController;
      class Scene;
      class NodeComposition;

      class ComponentContext
      {
      public:
        ComponentContext() : parent_(0), owner_(0), stateOwner_(0), boundary_(0), platformController_(0), scene_(0), window_(0), composition_(0), dirtyFlags_(NODE_DIRTY_NONE) {}
        explicit ComponentContext(ComponentContext *parent)
            : parent_(parent), owner_(0), stateOwner_(0), boundary_(0), platformController_(0), scene_(0), window_(0), composition_(0), dirtyFlags_(NODE_DIRTY_NONE) {}

        ComponentContext *parent() const { return parent_; }
        void setParent(ComponentContext *parent) { parent_ = parent; }
        Node *owner() const { return owner_; }
        void setOwner(Node *owner) { owner_ = owner; }
        IStateOwner *stateOwner() const { return stateOwner_; }
        void setStateOwner(IStateOwner *owner) { stateOwner_ = owner; }
        BoundaryNode *boundary() const { return boundary_; }
        void setBoundary(BoundaryNode *boundary) { boundary_ = boundary; }
        IPlatformController *platformController() const { return platformController_; }
        void setPlatformController(IPlatformController *controller) { platformController_ = controller; }
        Scene *scene() const { return scene_; }
        void setScene(Scene *scene) { scene_ = scene; }
        ::Window *window() const { return window_; }
        void setWindow(::Window *window) { window_ = window; }
        NodeComposition *composition() const { return composition_; }
        void setComposition(NodeComposition *comp) { composition_ = comp; }
        NodeDirtyFlags dirtyFlags() const { return dirtyFlags_; }
        void setDirtyFlags(NodeDirtyFlags flags) { dirtyFlags_ = flags; }

      private:
        friend class ComposableNode;
        ComponentContext *parent_;
        Node *owner_;
        IStateOwner *stateOwner_;
        BoundaryNode *boundary_;
        IPlatformController *platformController_;
        Scene *scene_;
        ::Window *window_;
        NodeComposition *composition_;
        NodeDirtyFlags dirtyFlags_;
      };

    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_CONTEXT_COMPONENTCONTEXT_HPP
