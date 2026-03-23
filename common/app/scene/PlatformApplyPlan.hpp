#ifndef LOKA_CORE2_SCENE_PLATFORMAPPLYPLAN_HPP
#define LOKA_CORE2_SCENE_PLATFORMAPPLYPLAN_HPP

namespace loka
{
  namespace app
  {
    namespace scene
    {
      class BoundaryNode;

      struct PlatformApplyPlan
      {
        enum PaintKind
        {
          PAINT_NONE = 0,
          PAINT_LOCAL = 1,
          PAINT_LOCAL_OPAQUE = 2,
          PAINT_COMPOSITED = 3
        };

        PlatformApplyPlan()
            : structureChanged(false),
              structureRoot(0),
              layoutChanged(false),
              paintKind(PAINT_NONE),
              layoutRoot(0),
              paintRoot(0)
        {
        }

        void clear()
        {
          structureChanged = false;
          structureRoot = 0;
          layoutChanged = false;
          paintKind = PAINT_NONE;
          layoutRoot = 0;
          paintRoot = 0;
        }

        PlatformApplyPlan forBoundary(BoundaryNode *boundary) const
        {
          PlatformApplyPlan localized = *this;
          if (boundary)
          {
            if (localized.structureChanged)
            {
              localized.structureRoot = boundary;
            }
            localized.layoutRoot = boundary;
            if (localized.paintKind != PAINT_NONE)
            {
              localized.paintRoot = boundary;
            }
          }
          return localized;
        }

        bool hasStructureWork() const
        {
          return structureChanged;
        }

        bool hasLocalStructureWork(const BoundaryNode *boundary) const
        {
          return structureChanged && structureRoot == boundary;
        }

        bool hasLayoutWork() const
        {
          return layoutChanged;
        }

        bool hasPaintWork() const
        {
          return paintKind != PAINT_NONE;
        }

        bool requiresCompositedPaint() const
        {
          return paintKind == PAINT_COMPOSITED;
        }

        bool isOpaqueLocalPaint() const
        {
          return paintKind == PAINT_LOCAL_OPAQUE;
        }

        bool hasLocalLayoutWork(const BoundaryNode *boundary) const
        {
          return layoutChanged && layoutRoot == boundary;
        }

        bool hasLocalPaintWork(const BoundaryNode *boundary) const
        {
          return paintKind != PAINT_NONE && paintRoot == boundary;
        }

        bool structureChanged;
        BoundaryNode *structureRoot;
        bool layoutChanged;
        PaintKind paintKind;
        BoundaryNode *layoutRoot;
        BoundaryNode *paintRoot;
      };
    }
  }
}

#endif
