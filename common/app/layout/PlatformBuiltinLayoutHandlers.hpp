#ifndef LOKA_APP_LAYOUT_PLATFORM_BUILTIN_LAYOUT_HANDLERS_HPP
#define LOKA_APP_LAYOUT_PLATFORM_BUILTIN_LAYOUT_HANDLERS_HPP

#include "app/layout/BoxLayout.hpp"
#include "app/layout/ColumnLayout.hpp"
#include "app/layout/GridLayout.hpp"
#include "app/layout/RowLayout.hpp"
#include "app/layout/ZStackLayout.hpp"
#include "app/scene/projection/PlatformLayoutHandler.hpp"

namespace loka
{
  namespace app
  {
    namespace layout
    {
      inline int DispatchTraversalLayoutChild(void *context,
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
          return loka::app::layout::computeBoxLayoutResultY(box, state, traversal, &DispatchTraversalLayoutChild);
        }
      };

      class ZStackPlatformLayoutHandler : public loka::app::scene::IPlatformLayoutHandler
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
            return state.y;
          }
          return loka::app::layout::computeZStackLayoutResultY(stack, state, traversal, &DispatchTraversalLayoutChild);
        }
      };

      class ColumnPlatformLayoutHandler : public loka::app::scene::IPlatformLayoutHandler
      {
      public:
        virtual const void *nodeTypeKey() const
        {
          return loka::app::scene::NodeTypeToken<loka::app::ColumnNode>();
        }

        virtual int layoutNode(loka::app::scene::Node *node,
                               const loka::app::scene::LayoutState &state,
                               loka::app::scene::IPlatformLayoutTraversal *traversal)
        {
          loka::app::ColumnNode *column = node ? node->asColumnNode() : 0;
          if (!column || !traversal)
          {
            return state.y;
          }
          return loka::app::layout::computeColumnLayoutResultY(column, state, traversal, &DispatchTraversalLayoutChild);
        }
      };

      class RowPlatformLayoutHandler : public loka::app::scene::IPlatformLayoutHandler
      {
      public:
        explicit RowPlatformLayoutHandler(const RowLayoutMetrics &metrics)
            : metrics_(metrics)
        {
        }

        virtual const void *nodeTypeKey() const
        {
          return loka::app::scene::NodeTypeToken<loka::app::RowNode>();
        }

        virtual int layoutNode(loka::app::scene::Node *node,
                               const loka::app::scene::LayoutState &state,
                               loka::app::scene::IPlatformLayoutTraversal *traversal)
        {
          loka::app::RowNode *row = node ? node->asRowNode() : 0;
          if (!row || !traversal)
          {
            return state.y;
          }
          return loka::app::layout::computeRowLayoutResultY(
              row, state, this->metrics_, traversal, &DispatchTraversalLayoutChild);
        }

      private:
        RowLayoutMetrics metrics_;
      };

      class GridPlatformLayoutHandler : public loka::app::scene::IPlatformLayoutHandler
      {
      public:
        explicit GridPlatformLayoutHandler(const GridLayoutMetrics &metrics)
            : metrics_(metrics)
        {
        }

        virtual const void *nodeTypeKey() const
        {
          return loka::app::scene::NodeTypeToken<loka::app::GridNode>();
        }

        virtual int layoutNode(loka::app::scene::Node *node,
                               const loka::app::scene::LayoutState &state,
                               loka::app::scene::IPlatformLayoutTraversal *traversal)
        {
          loka::app::GridNode *grid = node ? node->asGridNode() : 0;
          if (!grid || !traversal)
          {
            return state.y;
          }
          return loka::app::layout::computeGridLayoutResultY(
              grid, state, this->metrics_, traversal, &DispatchTraversalLayoutChild);
        }

      private:
        GridLayoutMetrics metrics_;
      };

      inline void RegisterBuiltinPlatformLayoutHandlers(loka::app::scene::PlatformLayoutHandlerRegistry &registry,
                                                        const RowLayoutMetrics *rowMetrics,
                                                        const GridLayoutMetrics *gridMetrics)
      {
        registry.registerHandler(new BoxPlatformLayoutHandler());
        registry.registerHandler(new ZStackPlatformLayoutHandler());
        registry.registerHandler(new ColumnPlatformLayoutHandler());
        if (rowMetrics)
        {
          registry.registerHandler(new RowPlatformLayoutHandler(*rowMetrics));
        }
        if (gridMetrics)
        {
          registry.registerHandler(new GridPlatformLayoutHandler(*gridMetrics));
        }
      }
    } // namespace layout
  } // namespace app
} // namespace loka

#endif // LOKA_APP_LAYOUT_PLATFORM_BUILTIN_LAYOUT_HANDLERS_HPP
