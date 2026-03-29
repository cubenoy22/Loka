#include "MacPlatformNodeHandlers.hpp"
#include "MacScenePlatformController.hpp"
#include "app/Cell.hpp"
#include "app/EditText.hpp"
#include "app/ImageView.hpp"
#include "app/OpenFileDialog.hpp"
#include "app/PopupMenu.hpp"
#include "app/Text.hpp"
#include "context/MacCellContext.hpp"
#include "context/MacEditTextContext.hpp"
#include "context/MacImageViewContext.hpp"
#include "context/MacOpenFileDialogContext.hpp"
#include "context/MacPopupMenuContext.hpp"
#include "context/MacTextContext.hpp"

namespace
{
  class MacTextNodeHandler : public loka::app::scene::IPlatformNodeHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<loka::app::TextNode>();
    }

    virtual loka::app::scene::NodeContext *ensureContext(loka::app::scene::Node *node,
                                                         loka::app::scene::IPlatformController *controller,
                                                         const loka::app::scene::LayoutState &state)
    {
      loka::app::TextNode *text = node ? node->asTextNode() : 0;
      MacScenePlatformController *mac = static_cast<MacScenePlatformController *>(controller);
      if (!text || !mac)
      {
        return 0;
      }
      return mac->contextMapper()->ensureTextContext(text,
                                                     state.x,
                                                     state.y,
                                                     state.width,
                                                     state.height);
    }
  };

  class MacImageViewNodeHandler : public loka::app::scene::IPlatformNodeHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<loka::app::ImageViewNode>();
    }

    virtual loka::app::scene::NodeContext *ensureContext(loka::app::scene::Node *node,
                                                         loka::app::scene::IPlatformController *controller,
                                                         const loka::app::scene::LayoutState &state)
    {
      loka::app::ImageViewNode *image = node ? node->asImageViewNode() : 0;
      MacScenePlatformController *mac = static_cast<MacScenePlatformController *>(controller);
      if (!image || !mac)
      {
        return 0;
      }
      return mac->contextMapper()->ensureImageViewContext(image,
                                                          state.x,
                                                          state.y,
                                                          state.width,
                                                          state.height);
    }
  };

  class MacEditTextNodeHandler : public loka::app::scene::IPlatformNodeHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<loka::app::EditTextNode>();
    }

    virtual loka::app::scene::NodeContext *ensureContext(loka::app::scene::Node *node,
                                                         loka::app::scene::IPlatformController *controller,
                                                         const loka::app::scene::LayoutState &state)
    {
      loka::app::EditTextNode *edit = node ? node->asEditTextNode() : 0;
      MacScenePlatformController *mac = static_cast<MacScenePlatformController *>(controller);
      if (!edit || !mac)
      {
        return 0;
      }
      return mac->contextMapper()->ensureEditTextContext(edit,
                                                         state.x,
                                                         state.y,
                                                         state.width,
                                                         state.height);
    }
  };

  class MacPopupMenuNodeHandler : public loka::app::scene::IPlatformNodeHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<loka::app::PopupMenuNode>();
    }

    virtual loka::app::scene::NodeContext *ensureContext(loka::app::scene::Node *node,
                                                         loka::app::scene::IPlatformController *controller,
                                                         const loka::app::scene::LayoutState &state)
    {
      loka::app::PopupMenuNode *popup = node ? node->asPopupMenuNode() : 0;
      MacScenePlatformController *mac = static_cast<MacScenePlatformController *>(controller);
      if (!popup || !mac)
      {
        return 0;
      }
      return mac->contextMapper()->ensurePopupMenuContext(popup,
                                                          state.x,
                                                          state.y,
                                                          state.width,
                                                          state.height);
    }
  };

  class MacCellNodeHandler : public loka::app::scene::IPlatformNodeHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<loka::app::CellNode>();
    }

    virtual loka::app::scene::NodeContext *ensureContext(loka::app::scene::Node *node,
                                                         loka::app::scene::IPlatformController *controller,
                                                         const loka::app::scene::LayoutState &state)
    {
      loka::app::CellNode *cell = node ? node->asCellNode() : 0;
      MacScenePlatformController *mac = static_cast<MacScenePlatformController *>(controller);
      if (!cell || !mac)
      {
        return 0;
      }
      return mac->contextMapper()->ensureCellContext(cell,
                                                     state.x,
                                                     state.y,
                                                     state.width,
                                                     state.height);
    }
  };

  class MacOpenFileDialogNodeHandler : public loka::app::scene::IPlatformNodeHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<loka::app::OpenFileDialogNode>();
    }

    virtual loka::app::scene::NodeContext *ensureContext(loka::app::scene::Node *node,
                                                         loka::app::scene::IPlatformController *controller,
                                                         const loka::app::scene::LayoutState &state)
    {
      (void)state;
      loka::app::OpenFileDialogNode *dialog = node ? node->asOpenFileDialogNode() : 0;
      MacScenePlatformController *mac = static_cast<MacScenePlatformController *>(controller);
      if (!dialog || !mac)
      {
        return 0;
      }
      return mac->contextMapper()->ensureOpenFileDialogContext(dialog);
    }
  };

  MacTextNodeHandler gMacTextNodeHandler;
  MacImageViewNodeHandler gMacImageViewNodeHandler;
  MacEditTextNodeHandler gMacEditTextNodeHandler;
  MacPopupMenuNodeHandler gMacPopupMenuNodeHandler;
  MacCellNodeHandler gMacCellNodeHandler;
  MacOpenFileDialogNodeHandler gMacOpenFileDialogNodeHandler;
}

void RegisterMacPlatformNodeHandlers(loka::app::scene::PlatformNodeHandlerRegistry &registry)
{
  registry.registerHandler(&gMacTextNodeHandler);
  registry.registerHandler(&gMacImageViewNodeHandler);
  registry.registerHandler(&gMacEditTextNodeHandler);
  registry.registerHandler(&gMacPopupMenuNodeHandler);
  registry.registerHandler(&gMacCellNodeHandler);
  registry.registerHandler(&gMacOpenFileDialogNodeHandler);
}
