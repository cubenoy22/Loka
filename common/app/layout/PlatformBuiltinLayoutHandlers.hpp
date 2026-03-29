#ifndef LOKA_APP_LAYOUT_PLATFORM_BUILTIN_LAYOUT_HANDLERS_HPP
#define LOKA_APP_LAYOUT_PLATFORM_BUILTIN_LAYOUT_HANDLERS_HPP

#include "app/layout/ContainerLayout.hpp"
#include "app/scene/PlatformLayoutHandler.hpp"

namespace loka
{
  namespace app
  {
    namespace layout
    {
      class BoxPlatformLayoutHandler : public loka::app::scene::IPlatformLayoutHandler
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
            return state.y;
          }
          return loka::app::layout::computeBoxLayoutResultY(box, state, traversal, &BoxPlatformLayoutHandler::layoutChild);
        }

      private:
        static int layoutChild(void *context,
                               loka::app::scene::Node *child,
                               const loka::app::scene::LayoutState &state)
        {
          loka::app::scene::IPlatformLayoutTraversal *traversal =
              static_cast<loka::app::scene::IPlatformLayoutTraversal *>(context);
          if (!traversal)
          {
            return state.y;
          }
          return traversal->layoutChild(child, state);
        }
      };

      inline void RegisterBuiltinPlatformLayoutHandlers(loka::app::scene::PlatformLayoutHandlerRegistry &registry)
      {
        registry.registerHandler(new BoxPlatformLayoutHandler());
      }
    }
  }
}

#endif // LOKA_APP_LAYOUT_PLATFORM_BUILTIN_LAYOUT_HANDLERS_HPP
