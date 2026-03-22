#ifndef LOKA_CORE2_SCENE_SCENEDIRECTOR_HPP
#define LOKA_CORE2_SCENE_SCENEDIRECTOR_HPP

#include "app/scene/Node.hpp"

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
        NodeDirtyFlags pendingBoundaryFlags() const;
        BoundaryNode *pendingBoundariesHead() const;
        NodeDirtyFlags aggregatePendingBoundaryFlags() const;
        BoundaryNode *topMostRequestedBoundary(BoundaryNode *boundary) const;
        bool isBoundaryUpdateRoot(BoundaryNode *boundary) const;
        BoundaryNode *firstPendingUpdateRoot() const;
        BoundaryNode *nextPendingUpdateRoot(BoundaryNode *afterRoot) const;
        void clearPendingBoundaryRequest();

      private:
        void enqueueBoundary(BoundaryNode *boundary);

        Scene *scene_;
        BoundaryNode *lastRequestedBoundary_;
        NodeDirtyFlags pendingBoundaryFlags_;
        BoundaryNode *pendingBoundariesHead_;
        BoundaryNode *pendingBoundariesTail_;
      };
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_SCENEDIRECTOR_HPP
