#ifndef LOKA_TOOLBOX_NODE_DISPATCH_HPP
#define LOKA_TOOLBOX_NODE_DISPATCH_HPP

#include "app/scene/Node.hpp"
#include "app/layout/StackSpans.hpp"
#include "app/scene/boundary/Boundary.hpp"

class ToolboxScenePlatformController;

short LayoutNode(loka::app::scene::Node *node,
                 loka::app::scene::LayoutState &state,
                 ToolboxScenePlatformController *controller,
                 loka::app::scene::BoundaryNode *currentBoundary,
                 const loka::app::layout::LazyWindow *range = 0,
                 loka::app::layout::StackSpans *spans = 0);
void RenderNode(loka::app::scene::Node *node,
                ToolboxScenePlatformController *controller);
short LayoutChildren(loka::app::scene::INestable *nestable,
                     loka::app::scene::LayoutState &state,
                     ToolboxScenePlatformController *controller,
                     loka::app::scene::BoundaryNode *currentBoundary);
void RenderChildren(loka::app::scene::INestable *nestable,
                    ToolboxScenePlatformController *controller);

#endif // LOKA_TOOLBOX_NODE_DISPATCH_HPP

