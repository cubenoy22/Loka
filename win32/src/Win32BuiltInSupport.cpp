#include "Win32BuiltInSupport.hpp"
#include "Win32ScenePlatformController.hpp"
#include "app/nodes/nestable/Grid.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/nestable/Box.hpp"
#include "app/nodes/nestable/ZStack.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/controls/Cell.hpp"
#include "app/nodes/controls/EditText.hpp"
#include "app/nodes/ImageView.hpp"
#include "app/OpenFileDialog.hpp"
#include "app/nodes/controls/PopupMenu.hpp"
#include "app/nodes/Text.hpp"
#include "app/layout/PlatformBuiltinLayoutHandlers.hpp"
#include "context/Win32ButtonContext.hpp"
#include "context/Win32CellContext.hpp"
#include "context/Win32EditTextContext.hpp"
#include "context/Win32ImageViewContext.hpp"
#include "context/Win32OpenFileDialogContext.hpp"
#include "context/Win32PopupMenuContext.hpp"
#include "context/Win32TextContext.hpp"

namespace
{
  const int kButtonHeight = 32;
  const int kEditTextHeight = 24;
  const int kPopupMenuHeight = 26;
  const int kTextHeight = 20;
  const int kHorizontalSpacing = 12;
  const int kImageFallbackHeightModern = 160;

  Win32ScenePlatformController::LayoutNodeResult
  DispatchProjectedLayout(Win32ScenePlatformController *controller,
                          loka::app::scene::Node *node,
                          const Win32ScenePlatformController::LayoutState &state)
  {
    if (!controller || !node)
    {
      return Win32ScenePlatformController::LayoutNodeResult(state.width, state.y);
    }
    loka::app::scene::IProjectedLayoutNode *projected = node->asProjectedLayoutNode();
    if (!projected)
    {
      return Win32ScenePlatformController::LayoutNodeResult(state.width, state.y);
    }
    loka::app::scene::LayoutState projectedState;
    projectedState.x = static_cast<short>(state.x);
    projectedState.y = static_cast<short>(state.y);
    projectedState.width = static_cast<short>(state.width);
    projectedState.height = static_cast<short>(state.height);
    projectedState.lineHeight = 0;
    projectedState.spacing = 0;
    return Win32ScenePlatformController::LayoutNodeResult(state.width,
                                                          projected->layoutProjected(controller, projectedState));
  }

  Win32ScenePlatformController::LayoutNodeResult
  DispatchTextLayout(Win32ScenePlatformController *controller,
                     loka::app::scene::Node *node,
                     const Win32ScenePlatformController::LayoutState &state)
  {
    return DispatchProjectedLayout(controller, node, state);
  }

  Win32ScenePlatformController::LayoutNodeResult
  DispatchImageViewLayout(Win32ScenePlatformController *controller,
                          loka::app::scene::Node *node,
                          const Win32ScenePlatformController::LayoutState &state)
  {
    return DispatchProjectedLayout(controller, node, state);
  }

  Win32ScenePlatformController::LayoutNodeResult
  DispatchButtonLayout(Win32ScenePlatformController *controller,
                       loka::app::scene::Node *node,
                       const Win32ScenePlatformController::LayoutState &state)
  {
    return DispatchProjectedLayout(controller, node, state);
  }

  Win32ScenePlatformController::LayoutNodeResult
  DispatchEditTextLayout(Win32ScenePlatformController *controller,
                         loka::app::scene::Node *node,
                         const Win32ScenePlatformController::LayoutState &state)
  {
    return DispatchProjectedLayout(controller, node, state);
  }

  Win32ScenePlatformController::LayoutNodeResult
  DispatchPopupMenuLayout(Win32ScenePlatformController *controller,
                          loka::app::scene::Node *node,
                          const Win32ScenePlatformController::LayoutState &state)
  {
    return DispatchProjectedLayout(controller, node, state);
  }

  Win32ScenePlatformController::LayoutNodeResult
  DispatchCellLayout(Win32ScenePlatformController *controller,
                     loka::app::scene::Node *node,
                     const Win32ScenePlatformController::LayoutState &state)
  {
    return DispatchProjectedLayout(controller, node, state);
  }

  Win32ScenePlatformController::LayoutNodeResult
  DispatchOpenFileDialogLayout(Win32ScenePlatformController *controller,
                               loka::app::scene::Node *node,
                               const Win32ScenePlatformController::LayoutState &state)
  {
    return DispatchProjectedLayout(controller, node, state);
  }
} // namespace

void RegisterWin32BuiltInSupport(Win32ScenePlatformController &controller)
{
  loka::app::layout::RowLayoutMetrics rowMetrics;
  rowMetrics.gap = kHorizontalSpacing;
  rowMetrics.fallbackHeight = kTextHeight;
  rowMetrics.buttonHeight = kButtonHeight;
  rowMetrics.editTextHeight = kEditTextHeight;
  rowMetrics.popupMenuHeight = kPopupMenuHeight;
  rowMetrics.textHeight = kTextHeight;
  rowMetrics.imageFallbackHeight = kImageFallbackHeightModern;
  loka::app::layout::GridLayoutMetrics gridMetrics;
  gridMetrics.gapX = 0;
  gridMetrics.gapY = 0;
  loka::app::layout::RegisterBuiltinPlatformLayoutHandlers(
      controller.layoutHandlerRegistry_, &rowMetrics, &gridMetrics);
  controller.leafLayoutHandlerRegistry_.registerHandler(loka::app::scene::NodeTypeToken<loka::app::ButtonNode>(),
                                                        &DispatchButtonLayout);
  controller.leafLayoutHandlerRegistry_.registerHandler(loka::app::scene::NodeTypeToken<loka::app::EditTextNode>(),
                                                        &DispatchEditTextLayout);
  controller.leafLayoutHandlerRegistry_.registerHandler(loka::app::scene::NodeTypeToken<loka::app::PopupMenuNode>(),
                                                        &DispatchPopupMenuLayout);
  controller.leafLayoutHandlerRegistry_.registerHandler(loka::app::scene::NodeTypeToken<loka::app::CellNode>(),
                                                        &DispatchCellLayout);
  controller.leafLayoutHandlerRegistry_.registerHandler(loka::app::scene::NodeTypeToken<loka::app::TextNode>(),
                                                        &DispatchTextLayout);
  controller.leafLayoutHandlerRegistry_.registerHandler(loka::app::scene::NodeTypeToken<loka::app::ImageViewNode>(),
                                                        &DispatchImageViewLayout);
  controller.hostActionHandlerRegistry_.registerHandler(
      loka::app::scene::NodeTypeToken<loka::app::OpenFileDialogNode>(), &DispatchOpenFileDialogLayout);
  RegisterWin32ButtonNodeHandler(controller.nodeHandlerRegistry_);
  RegisterWin32TextNodeHandler(controller.nodeHandlerRegistry_);
  RegisterWin32ImageViewNodeHandler(controller.nodeHandlerRegistry_);
  RegisterWin32EditTextNodeHandler(controller.nodeHandlerRegistry_);
  RegisterWin32PopupMenuNodeHandler(controller.nodeHandlerRegistry_);
  RegisterWin32CellNodeHandler(controller.nodeHandlerRegistry_);
  RegisterWin32OpenFileDialogNodeHandler(controller.nodeHandlerRegistry_);
}
