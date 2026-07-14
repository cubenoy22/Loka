#ifndef LOKA_CORE2_SCENE_BOUNDARY_DETAIL_BOUNDARYRUNTIMESTATE_HPP
#define LOKA_CORE2_SCENE_BOUNDARY_DETAIL_BOUNDARYRUNTIMESTATE_HPP

#include "app/scene/boundary/BoundaryStateTypes.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      class Scene;
      class BoundaryNode;

      struct BoundaryRuntimeState
      {
        BoundaryRuntimeState()
            : scene(0),
              parentBoundary(0),
              layoutBounds(),
              frozen(false)
        {
        }

        void setFrozen(bool value)
        {
          frozen = value;
        }

        bool isFrozen() const
        {
          return frozen;
        }

        void setScene(Scene *value)
        {
          scene = value;
        }

        Scene *currentScene() const
        {
          return scene;
        }

        bool hasScene() const
        {
          return scene != 0;
        }

        void setParentBoundary(BoundaryNode *value)
        {
          parentBoundary = value;
        }

        BoundaryNode *currentParentBoundary() const
        {
          return parentBoundary;
        }

        bool hasParentBoundary() const
        {
          return parentBoundary != 0;
        }

        bool setLayoutBounds(int x, int y, int width, int height)
        {
          const int normalizedWidth = width < 0 ? 0 : width;
          const int normalizedHeight = height < 0 ? 0 : height;
          const bool changed = !layoutBounds.valid || layoutBounds.x != x || layoutBounds.y != y
                               || layoutBounds.width != normalizedWidth || layoutBounds.height != normalizedHeight;
          layoutBounds.set(x, y, normalizedWidth, normalizedHeight);
          return changed;
        }

        bool clearLayoutBounds()
        {
          const bool changed = layoutBounds.valid;
          layoutBounds.clear();
          return changed;
        }

        const BoundaryUpdateResult::BoundsHint &currentLayoutBounds() const
        {
          return layoutBounds;
        }

        bool hasLayoutBounds() const
        {
          return layoutBounds.valid;
        }

        Scene *scene;
        BoundaryNode *parentBoundary;
        BoundaryUpdateResult::BoundsHint layoutBounds;
        bool frozen;
      };
    } // namespace scene
  } // namespace app
} // namespace loka

#endif
