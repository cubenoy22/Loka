#ifndef LOKA_TEST_TOOLBOX_HOST_HPP
#define LOKA_TEST_TOOLBOX_HOST_HPP
// Replace OS/controller neighbors; compile the actual context, table, measure
// scope, lifecycle base and built-in registration source without alteration.
#define LOKA_TOOLBOX_SCENE_PLATFORM_CONTROLLER_HPP
#define LOKA_TOOLBOX_WINDOW_HPP
#define LOKA_TOOLBOX_WINDOW_CONTEXT_HPP
#define LOKA_TOOLBOX_APP_HPP
#define LOKA_TOOLBOX_BUTTON_CONTEXT_HPP
#define LOKA_TOOLBOX_CELL_CONTEXT_HPP
#define LOKA_TOOLBOX_EDIT_TEXT_CONTEXT_HPP
#define LOKA_TOOLBOX_IMAGE_VIEW_CONTEXT_HPP
#define LOKA_TOOLBOX_OPEN_FILE_DIALOG_CONTEXT_HPP
#define LOKA_TOOLBOX_POPUP_MENU_CONTEXT_HPP
#define LOKA_TOOLBOX_SCROLL_BAR_CONTEXT_HPP
#define LOKA_TOOLBOX_TEXT_CONTEXT_HPP
#include "Quickdraw.h"
#include "TextEdit.h"
#include "ToolboxEditControlLedger.hpp"
class ToolboxTextEditorContext;
#include "ToolboxCompositionReplay.hpp"
#include "app/scene/projection/PlatformController.hpp"
#include "app/scene/projection/PlatformNodeHandler.hpp"
#include "app/scene/projection/NativeNodeContext.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/controls/EditText.hpp"
#include "app/nodes/controls/ScrollBar.hpp"
#include "app/layout/TextShaping.hpp"
#include <vector>
#include <string>

struct CursorOwner
{
  int depth, entries;
  CursorOwner()
      : depth(0),
        entries(0)
  {
  }
};
class BusyScope
{
public:
  explicit BusyScope(CursorOwner *owner)
      : owner_(owner)
  {
    if (owner_)
    {
      ++owner_->depth;
      ++owner_->entries;
    }
  }
  ~BusyScope()
  {
    if (owner_)
      --owner_->depth;
  }

private:
  CursorOwner *owner_;
};
class ToolboxWindowContext
{
public:
  enum
  {
    CAP_CONTROL_MANAGER = 1,
    CAP_TEXT_EDIT = 2
  };
  int capabilities() const
  {
    return CAP_TEXT_EDIT;
  }
};
class ToolboxWindow
{
public:
  GrafPort port;
  ToolboxWindowContext context_;
  ToolboxWindow()
  {
    port.txFont = 3;
    port.txSize = 12;
    port.txFace = 0;
  }
  void requestInvalidateRect(const Rect &) {}
  GrafPtr window()
  {
    return &port;
  }
  ToolboxWindowContext *context()
  {
    return &context_;
  }
};
class ToolboxScenePlatformController : public loka::app::scene::IPlatformController
{
public:
  struct EditTextControlBinding
  {
    loka::app::scene::NodeContext *ownerContext;
    TEHandle te;
    void *text;
    ToolboxTextEditorContext *editor;
    Rect rect;
    bool usedThisFrame;
    loka::app::scene::NativeLifetimeHint lifetimeHint;
  };
  ToolboxEditControlLedger<EditTextControlBinding, loka::app::scene::NodeContext> editControls_;
  std::vector<TEHandle> retiredTE;
  TEHandle ensureTextEditorControl(ToolboxTextEditorContext *, const Rect &, loka::app::scene::NativeLifetimeHint);
  void retireTextEditorControl(loka::app::scene::NodeContext *, loka::app::scene::NativeLifetimeHint);
  void flushTE();
  ToolboxCompositionReplay compositionReplay;
  void registerCompositionReplay(ToolboxCompositionReplay::Registration &registration)
  {
    registration.attach(this->compositionReplay);
  }
  ToolboxWindow *window_;
  loka::app::scene::PlatformNodeHandlerRegistry nodeHandlerRegistry_;
  mutable CursorOwner cursor;
  std::vector<loka::app::scene::NodeContext *> retired;
  Rect projectionClip;
  loka::app::scene::NodeContext *renderContext;
  explicit ToolboxScenePlatformController(ToolboxWindow *window)
      : window_(window),
        renderContext(0)
  {
    SetRect(&projectionClip, -30000, -30000, 30000, 30000);
  }
  loka::app::TextShaping textShaping() const
  {
    return loka::app::PER_RUN;
  }
  CursorOwner *cursorOwner() const
  {
    return &cursor;
  }
  bool intersectWithProjectionClip(const Rect &rect, Rect &out) const;
  void retireNodeContext(loka::app::scene::NodeContext *context, loka::app::scene::NativeLifetimeHint)
  {
    retired.push_back(context);
  }
  virtual void onChange(loka::app::scene::Node *, loka::app::scene::NodeDirtyFlags, bool) {}
  void render()
  {
    if (this->renderContext)
      this->renderContext->render(this);
  }
  void drawControlsInRect(const Rect &) {}
  virtual void synchronize() {}
  virtual bool hasPendingSync() const
  {
    return false;
  }
  virtual void destroy() {}
};
#define LOKA_HOST_OTHER_HANDLER(Name)                                                                                  \
  inline bool RegisterToolbox##Name##NodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &)                      \
  {                                                                                                                    \
    return true;                                                                                                       \
  }
LOKA_HOST_OTHER_HANDLER(Button)
LOKA_HOST_OTHER_HANDLER(Cell)
LOKA_HOST_OTHER_HANDLER(EditText)
LOKA_HOST_OTHER_HANDLER(ImageView)
LOKA_HOST_OTHER_HANDLER(OpenFileDialog)
LOKA_HOST_OTHER_HANDLER(PopupMenu)
LOKA_HOST_OTHER_HANDLER(ScrollBar)
LOKA_HOST_OTHER_HANDLER(Text)
#undef LOKA_HOST_OTHER_HANDLER

namespace toolbox_host
{
  struct Draw
  {
    short x, y, size;
    Style face;
    int length;
    std::string bytes;
  };
  extern std::vector<Draw> draws;
  extern int erases, widths, measures, fonts, metrics;
  /** Remaining NewRgn calls to refuse with a null handle (Classic memory pressure). */
  extern int failRegions;
  void reset();
} // namespace toolbox_host
#endif
