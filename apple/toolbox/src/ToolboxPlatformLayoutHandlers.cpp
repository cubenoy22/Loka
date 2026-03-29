#include "ToolboxPlatformLayoutHandlers.hpp"

#include "app/Box.hpp"
#include "app/ZStack.hpp"
#include "loka/dsl/CompositionList.hpp"

namespace
{
  inline int DispatchTraversalLayoutChild(void *context,
                                          loka::app::scene::Node *child,
                                          const loka::app::scene::LayoutState &state)
  {
    loka::app::scene::IPlatformLayoutTraversal *traversal =
        static_cast<loka::app::scene::IPlatformLayoutTraversal *>(context);
    if (!traversal)
    {
      return 0;
    }
    return traversal->layoutChild(child, state);
  }

  class ToolboxBoxLayoutHandler : public loka::app::scene::IPlatformLayoutHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<loka::app::BoxNode>();
    }

    virtual int layoutNode(loka::app::scene::Node *node,
                           const loka::app::scene::LayoutState &state,
                           loka::app::scene::IPlatformLayoutTraversal *traversal)
    {
      loka::app::BoxNode *box = node ? node->asBoxNode() : 0;
      if (!box || !traversal)
      {
        return 0;
      }

      const short padding = static_cast<short>(box->props.padding);
      loka::app::scene::LayoutState childState = state;
      childState.x = static_cast<short>(state.x + padding);
      childState.y = static_cast<short>(state.y + padding);
      if (childState.width > 0)
      {
        childState.width = static_cast<short>(childState.width - padding * 2);
        if (childState.width < 0)
        {
          childState.width = 0;
        }
      }
      if (childState.height > 0)
      {
        childState.height = static_cast<short>(childState.height - padding * 2);
        if (childState.height < 0)
        {
          childState.height = 0;
        }
      }

      short childWidth = 0;
      if (loka::app::scene::INestable *nestable = box->asNestable())
      {
        loka::dsl::CompositionCursor<loka::app::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
        for (loka::app::scene::Node *child = it.next(); child; child = it.next())
        {
          const int width = DispatchTraversalLayoutChild(traversal, child, childState);
          if (width > childWidth)
          {
            childWidth = static_cast<short>(width);
          }
        }
      }

      short width = static_cast<short>(childWidth + padding * 2);
      if (childWidth == 0 && box->childrenCount() == 0)
      {
        width = static_cast<short>(padding * 2);
      }
      return width;
    }
  };

  class ToolboxZStackLayoutHandler : public loka::app::scene::IPlatformLayoutHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<loka::app::ZStackNode>();
    }

    virtual int layoutNode(loka::app::scene::Node *node,
                           const loka::app::scene::LayoutState &state,
                           loka::app::scene::IPlatformLayoutTraversal *traversal)
    {
      loka::app::ZStackNode *stack = node ? node->asZStackNode() : 0;
      if (!stack || !traversal)
      {
        return 0;
      }

      short maxWidth = 0;
      if (loka::app::scene::INestable *nestable = stack->asNestable())
      {
        loka::dsl::CompositionCursor<loka::app::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
        for (loka::app::scene::Node *child = it.next(); child; child = it.next())
        {
          loka::app::scene::LayoutState childState = state;
          const int width = DispatchTraversalLayoutChild(traversal, child, childState);
          if (width > maxWidth)
          {
            maxWidth = static_cast<short>(width);
          }
        }
      }
      return maxWidth;
    }
  };
}

void RegisterToolboxPlatformLayoutHandlers(loka::app::scene::PlatformLayoutHandlerRegistry &registry)
{
  registry.registerHandler(new ToolboxBoxLayoutHandler());
  registry.registerHandler(new ToolboxZStackLayoutHandler());
}
