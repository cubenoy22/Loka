#ifndef LOKA_APP_LAYOUT_ZSTACK_LAYOUT_HPP
#define LOKA_APP_LAYOUT_ZSTACK_LAYOUT_HPP

#include "app/ZStack.hpp"
#include "loka/dsl/CompositionList.hpp"

namespace loka
{
  namespace app
  {
    namespace layout
    {
      template <typename LayoutStateT>
      int computeZStackLayoutResultY(loka::app::ZStackNode *stack,
                                     const LayoutStateT &state,
                                     void *context,
                                     int (*layoutChild)(void *, loka::app::scene::Node *, const LayoutStateT &))
      {
        if (!stack)
        {
          return state.y;
        }

        int maxY = state.y;
        if (loka::app::scene::INestable *nestable = stack->asNestable())
        {
          loka::dsl::CompositionCursor<loka::app::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
          for (loka::app::scene::Node *child = it.next(); child; child = it.next())
          {
            LayoutStateT childState = state;
            const int childY = layoutChild(context, child, childState);
            if (childY > maxY)
            {
              maxY = childY;
            }
          }
        }
        return maxY;
      }
    }
  }
}

#endif // LOKA_APP_LAYOUT_ZSTACK_LAYOUT_HPP
