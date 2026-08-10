#ifndef LOKA_TOOLBOX_PLATFORM_LAYOUT_HANDLERS_HPP
#define LOKA_TOOLBOX_PLATFORM_LAYOUT_HANDLERS_HPP

#include "app/scene/projection/PlatformLayoutHandler.hpp"

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
