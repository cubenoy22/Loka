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

        void requestBoundaryUpdate(BoundaryNode *boundary, NodeDirtyFlags flags, bool flushImmediately);

        BoundaryNode *lastRequestedBoundary() const;
        NodeDirtyFlags pendingBoundaryFlags() const;
        void clearPendingBoundaryRequest();

      private:
        Scene *scene_;
        BoundaryNode *lastRequestedBoundary_;
        NodeDirtyFlags pendingBoundaryFlags_;
      };
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_SCENEDIRECTOR_HPP
