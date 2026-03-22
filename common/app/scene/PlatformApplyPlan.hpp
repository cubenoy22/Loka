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
          PAINT_COMPOSITED = 2
        };

        PlatformApplyPlan()
            : structureChanged(false),
              layoutChanged(false),
              paintKind(PAINT_NONE),
              layoutRoot(0),
              paintRoot(0)
        {
        }

        void clear()
        {
          structureChanged = false;
          layoutChanged = false;
          paintKind = PAINT_NONE;
          layoutRoot = 0;
          paintRoot = 0;
        }

        bool structureChanged;
        bool layoutChanged;
        PaintKind paintKind;
        BoundaryNode *layoutRoot;
        BoundaryNode *paintRoot;
      };
    }
  }
}

#endif
