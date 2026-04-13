#ifndef LOKA_TOOLBOX_NODE_CONTEXT_MAPPER_HPP
#define LOKA_TOOLBOX_NODE_CONTEXT_MAPPER_HPP

#include "app/nodes/controls/Button.hpp"
#include "app/nodes/controls/Cell.hpp"
#include "app/nodes/controls/EditText.hpp"
#include "app/OpenFileDialog.hpp"
#include "app/nodes/controls/PopupMenu.hpp"
#include "app/Text.hpp"
#include "app/ImageView.hpp"
#include "app/RectSurface.hpp"
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

  bool ensureProjectedContext(loka::app::scene::Node *node,
                              loka::app::scene::BoundaryNode *boundary);
  void ensureTextContext(loka::app::TextNode *node);
  void ensureCellContext(loka::app::CellNode *node);
  void ensureButtonContext(loka::app::ButtonNode *node);
  void ensureEditTextContext(loka::app::EditTextNode *node);
  void ensureOpenFileDialogContext(loka::app::OpenFileDialogNode *node);
  void ensurePopupMenuContext(loka::app::PopupMenuNode *node);
  void ensureImageViewContext(loka::app::ImageViewNode *node);
  void ensureRectSurfaceContext(loka::app::RectSurfaceNode *node);

private:
  int capabilities_;
};

#endif // LOKA_TOOLBOX_NODE_CONTEXT_MAPPER_HPP
