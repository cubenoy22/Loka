#ifndef LOKA_APP_NODES_BOUNDARY_RECOMPOSING_BOUNDARY_HPP
#define LOKA_APP_NODES_BOUNDARY_RECOMPOSING_BOUNDARY_HPP

#include "app/nodes/boundary/StdComposition.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      /** Re-declares a boundary's composition on its own CHILD dirt. */
      template <class NodeT, class Base> class RecomposingBoundaryFor : public Base
      {
      public:
        typedef typename Base::PropsType PropsType;
        explicit RecomposingBoundaryFor(const PropsType &p) : Base(p) {}

      protected:
        /** Local recompose declares the same composition attach did. */
        virtual void declareLocalRecomposition(NodeComposition &c)
        {
          this->composeNode(c);
        }

        virtual void composeWithContext(ComponentContext &ctx, ComposeEvent ev)
        {
          if (ev == COMPOSE_EVENT_UPDATE && (ctx.dirtyFlags() & NODE_DIRTY_CHILD))
          {
            this->recomposeLocally(ctx, ev);
            return;
          }
          Base::composeWithContext(ctx, ev);
        }

      private:
        /** The one seam where a future replacement pass (redraw / relayout /
            realloc style) would be selected. Today: diff with retain fast paths,
            full fallback when the diff cannot be applied. Not virtual: apps do
            not choose the strategy. */
        void recomposeLocally(ComponentContext &ctx, ComposeEvent ev)
        {
          if (this->recomposeLocalComposition(
                  ctx, ev, this->LOCAL_RECOMPOSE_APPLY_DIFF_WITH_RETAIN_FAST_PATHS))
          {
            return;
          }
          // A refused allocation is a recorded refusal, not a reason to tear the
          // live subtree down: the full fallback detaches and retires every
          // child before it tries to create the replacement, and under memory
          // pressure that replacement may not materialize (bot P1 on #620;
          // the MineSweeper form this base replaces kept the same guard).
          if (this->composeResult().allocationFailed)
          {
            return;
          }
          this->recomposeLocalCompositionWithFullFallback(
              ctx, ev, this->LOCAL_RECOMPOSE_APPLY_DIFF_WITH_RETAIN_FAST_PATHS);
        }
      };
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_APP_NODES_BOUNDARY_RECOMPOSING_BOUNDARY_HPP
