#ifndef LOKA_APP_LAYOUT_BOX_LAYOUT_HPP
#define LOKA_APP_LAYOUT_BOX_LAYOUT_HPP

#include "app/nodes/nestable/Box.hpp"
#include "app/layout/LayoutHeuristics.hpp"
#include "dsl/composition/CompositionList.hpp"

namespace loka
{
  namespace app
  {
    namespace layout
    {
      template <typename LayoutStateT>
      int computeBoxLayoutResultY(loka::app::BoxNode *box,
                                  const LayoutStateT &state,
                                  void *context,
                                  int (*layoutChild)(void *, loka::app::scene::Node *, const LayoutStateT &))
      {
        if (!box)
        {
          return state.y;
        }

        const int padding = box->props.padding;
        const bool hasFixedSize = box->props.hasFixedSize();
        LayoutStateT childState = state;
        childState.x = layoutCoordinate<LayoutStateT>(state.x + padding);
        childState.y = layoutCoordinate<LayoutStateT>(state.y + padding);
        childState.width = layoutCoordinate<LayoutStateT>(
            (hasFixedSize ? box->props.width : state.width) - padding * 2);
        if (childState.width < 0)
        {
          childState.width = 0;
        }
        childState.height = layoutCoordinate<LayoutStateT>(
            (hasFixedSize ? box->props.height : state.height) - padding * 2);
        if (childState.height < 0)
        {
          childState.height = 0;
        }

        int resultY = childState.y;
        if (loka::app::scene::INestable *nestable = box->asNestable())
        {
          loka::dsl::CompositionCursor<loka::app::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
          for (loka::app::scene::Node *child = it.next(); child; child = it.next())
          {
            childState.y = layoutCoordinate<LayoutStateT>(layoutChild(context, child, childState));
          }
          resultY = childState.y + padding;
        }
        else
        {
          resultY = state.y + padding * 2;
        }
        return hasFixedSize ? state.y + box->props.height : resultY;
      }
    } // namespace layout
  } // namespace app
} // namespace loka

#endif // LOKA_APP_LAYOUT_BOX_LAYOUT_HPP
