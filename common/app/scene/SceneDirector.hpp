#ifndef LOKA_CORE2_SCENE_SCENEDIRECTOR_HPP
#define LOKA_CORE2_SCENE_SCENEDIRECTOR_HPP

#include "app/scene/Node.hpp"
#include "app/scene/SceneProjectionTransaction.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      class Scene;
      class BoundaryNode;

      class SceneDirector
      {
      public:
        SceneDirector();

        void attach(Scene *scene);
        void detach();

        void registerBoundaryUpdate(BoundaryNode *boundary, NodeDirtyFlags flags);
        void requestBoundaryUpdate(BoundaryNode *boundary, NodeDirtyFlags flags, bool flushImmediately);

        BoundaryNode *lastRequestedBoundary() const;
        const SceneProjectionTransaction &projectionTransaction() const;
        BoundaryNode *pendingBoundariesHead() const;
        BoundaryNode *topMostRequestedBoundary(BoundaryNode *boundary) const;
        bool isBoundaryUpdateRoot(BoundaryNode *boundary) const;
        BoundaryNode *firstPendingUpdateRoot() const;
        BoundaryNode *nextPendingUpdateRoot(BoundaryNode *afterRoot) const;
        void clearPendingBoundaryRequest();

      private:
        void enqueueBoundary(BoundaryNode *boundary);

        Scene *scene_;
        BoundaryNode *lastRequestedBoundary_;
        SceneProjectionTransaction projectionTransaction_;
        BoundaryNode *pendingBoundariesHead_;
        BoundaryNode *pendingBoundariesTail_;
      };
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_SCENEDIRECTOR_HPP
