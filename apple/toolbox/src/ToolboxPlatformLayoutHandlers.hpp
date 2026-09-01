#ifndef LOKA_TOOLBOX_PLATFORM_LAYOUT_HANDLERS_HPP
#define LOKA_TOOLBOX_PLATFORM_LAYOUT_HANDLERS_HPP

#include "app/scene/projection/PlatformLayoutHandler.hpp"

namespace loka
{
  namespace app
  {
    class StackNode;
  }
} // namespace loka

/** Applies Toolbox Row layout through the supplied traversal. */
int ComputeToolboxRowLayout(loka::app::StackNode *row,
                            const loka::app::scene::LayoutState &state,
                            loka::app::scene::IPlatformLayoutTraversal *traversal);

/**
  Applies a registered Toolbox layout handler and commits both of its result
  channels to the caller's layout state.
*/
bool ApplyToolboxPlatformLayoutHandler(
    loka::app::scene::PlatformLayoutHandlerRegistry &registry,
    loka::app::scene::Node &node,
    loka::app::scene::LayoutState &state,
    loka::app::scene::IPlatformLayoutTraversal &traversal,
    short &width);

void RegisterToolboxPlatformLayoutHandlers(loka::app::scene::PlatformLayoutHandlerRegistry &registry);

#endif // LOKA_TOOLBOX_PLATFORM_LAYOUT_HANDLERS_HPP
