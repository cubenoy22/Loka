#ifndef LOKA_TOOLBOX_NODE_CONTEXT_MAPPER_HPP
#define LOKA_TOOLBOX_NODE_CONTEXT_MAPPER_HPP

#include "app/Button.hpp"
#include "app/EditText.hpp"
#include "app/PopupMenu.hpp"
#include "app/Text.hpp"
#include <Quickdraw.h>

class ToolboxScenePlatformController;

class ToolboxNodeContextMapper
{
public:
  ToolboxNodeContextMapper() {}

  void ensureTextContext(declara::app::TextNode *node,
                         const Rect &rect,
                         short textX,
                         short textY,
                         ToolboxScenePlatformController *controller);
  void ensureButtonContext(declara::app::ButtonNode *node,
                           const Rect &rect,
                           const loka::core::String &label,
                           ToolboxScenePlatformController *controller);
  void ensureEditTextContext(declara::app::EditTextNode *node,
                             const Rect &rect,
                             const Rect &textRect,
                             short textX,
                             short textY,
                             ToolboxScenePlatformController *controller);
  void ensurePopupMenuContext(declara::app::PopupMenuNode *node,
                              const Rect &rect,
                              short lineHeight,
                              ToolboxScenePlatformController *controller);
};

#endif // LOKA_TOOLBOX_NODE_CONTEXT_MAPPER_HPP
