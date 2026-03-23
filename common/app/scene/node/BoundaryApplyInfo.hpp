#ifndef LOKA_CORE2_SCENE_NODE_BOUNDARYAPPLYINFO_HPP
#define LOKA_CORE2_SCENE_NODE_BOUNDARYAPPLYINFO_HPP

#include "BoundaryStateTypes.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      enum BoundaryLocalApplyPaintKind
      {
        LOCAL_APPLY_PAINT_NONE = 0,
        LOCAL_APPLY_PAINT_GENERIC = 1,
        LOCAL_APPLY_PAINT_OPAQUE = 2,
        LOCAL_APPLY_PAINT_COMPOSITED = 3
      };

      enum BoundaryLocalApplyBoundsKind
      {
        LOCAL_APPLY_BOUNDS_NONE = 0,
        LOCAL_APPLY_BOUNDS_LAYOUT = 1,
        LOCAL_APPLY_BOUNDS_PAINT = 2
      };

      struct BoundaryLocalApplyInfo
      {
        typedef BoundaryUpdateResult::BoundsHint LayoutBounds;

        BoundaryLocalApplyInfo()
            : isLocalStructureRoot(false),
              isLocalLayoutRoot(false),
              isLocalPaintRoot(false),
              hasStructureWork(false),
              hasLayoutWork(false),
              paintKind(LOCAL_APPLY_PAINT_NONE),
              boundsKind(LOCAL_APPLY_BOUNDS_NONE),
              bounds(0),
              usesPaintBoundsHint(false),
              hasPaintSpecificBoundsHint(false),
              hasOpaqueCoverageHint(false),
              paintIsOpaque(false)
        {
        }

        bool hasPaintWork() const
        {
          return paintKind != LOCAL_APPLY_PAINT_NONE;
        }

        bool hasAnyWork() const
        {
          return hasStructureWork || hasLayoutWork || hasPaintWork();
        }

        bool hasBoundsHint() const
        {
          return bounds != 0;
        }

        bool hasLayoutBoundsHint() const
        {
          return boundsKind == LOCAL_APPLY_BOUNDS_LAYOUT;
        }

        bool hasPaintBoundsHint() const
        {
          return boundsKind == LOCAL_APPLY_BOUNDS_PAINT;
        }

        bool hasOpaquePaintWork() const
        {
          return paintKind == LOCAL_APPLY_PAINT_OPAQUE;
        }

        bool hasGenericPaintWork() const
        {
          return paintKind == LOCAL_APPLY_PAINT_GENERIC;
        }

        bool hasCompositedPaintWork() const
        {
          return paintKind == LOCAL_APPLY_PAINT_COMPOSITED;
        }

        bool hasRootedWork() const
        {
          return isLocalStructureRoot || isLocalLayoutRoot || isLocalPaintRoot;
        }

        bool isLocalStructureRoot;
        bool isLocalLayoutRoot;
        bool isLocalPaintRoot;
        bool hasStructureWork;
        bool hasLayoutWork;
        BoundaryLocalApplyPaintKind paintKind;
        BoundaryLocalApplyBoundsKind boundsKind;
        const LayoutBounds *bounds;
        bool usesPaintBoundsHint;
        bool hasPaintSpecificBoundsHint;
        bool hasOpaqueCoverageHint;
        bool paintIsOpaque;
      };
    }
  }
}

#endif
