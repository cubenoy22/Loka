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
  enum Capability
  {
    CAP_NONE = 0,
    CAP_CONTROL_MANAGER = 1 << 0,
    CAP_TEXT_EDIT = 1 << 1
  };

  explicit ToolboxNodeContextMapper(int capabilities)
      : capabilities_(capabilities) {}

  int capabilities() const { return capabilities_; }
  bool hasCapability(Capability cap) const { return (capabilities_ & cap) != 0; }

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
  void renderTextContext(declara::app::TextNode *node,
                         ToolboxScenePlatformController *controller);
  void renderButtonContext(declara::app::ButtonNode *node,
                           ToolboxScenePlatformController *controller);
  void renderEditTextContext(declara::app::EditTextNode *node,
                             ToolboxScenePlatformController *controller);
  void renderPopupMenuContext(declara::app::PopupMenuNode *node);

private:
  int capabilities_;
};

#endif // LOKA_TOOLBOX_NODE_CONTEXT_MAPPER_HPP
