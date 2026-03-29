#include "Win32BuiltInSupport.hpp"
#include "Win32ScenePlatformController.hpp"
#include "app/Grid.hpp"
#include "app/RowColumn.hpp"
#include "app/Box.hpp"
#include "app/ZStack.hpp"
#include "app/Button.hpp"
#include "app/Cell.hpp"
#include "app/EditText.hpp"
#include "app/ImageView.hpp"
#include "app/OpenFileDialog.hpp"
#include "app/PopupMenu.hpp"
#include "app/Text.hpp"
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

  Win32ScenePlatformController::LayoutNodeResult DispatchProjectedLayout(
      Win32ScenePlatformController *controller,
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
    return Win32ScenePlatformController::LayoutNodeResult(
        state.width,
        projected->layoutProjected(controller, projectedState));
  }
}

Win32ScenePlatformController::LayoutNodeResult Win32ScenePlatformController::dispatchTextLayout(
    Win32ScenePlatformController *controller,
    loka::app::scene::Node *node,
    const LayoutState &state)
{
  return DispatchProjectedLayout(controller, node, state);
}

Win32ScenePlatformController::LayoutNodeResult Win32ScenePlatformController::dispatchImageViewLayout(
    Win32ScenePlatformController *controller,
    loka::app::scene::Node *node,
    const LayoutState &state)
{
  return DispatchProjectedLayout(controller, node, state);
}

Win32ScenePlatformController::LayoutNodeResult Win32ScenePlatformController::dispatchButtonLayout(
    Win32ScenePlatformController *controller,
    loka::app::scene::Node *node,
    const LayoutState &state)
{
  return DispatchProjectedLayout(controller, node, state);
}

Win32ScenePlatformController::LayoutNodeResult Win32ScenePlatformController::dispatchEditTextLayout(
    Win32ScenePlatformController *controller,
    loka::app::scene::Node *node,
    const LayoutState &state)
{
  return DispatchProjectedLayout(controller, node, state);
}

Win32ScenePlatformController::LayoutNodeResult Win32ScenePlatformController::dispatchPopupMenuLayout(
    Win32ScenePlatformController *controller,
    loka::app::scene::Node *node,
    const LayoutState &state)
{
  return DispatchProjectedLayout(controller, node, state);
}

Win32ScenePlatformController::LayoutNodeResult Win32ScenePlatformController::dispatchCellLayout(
    Win32ScenePlatformController *controller,
    loka::app::scene::Node *node,
    const LayoutState &state)
{
  return DispatchProjectedLayout(controller, node, state);
}

Win32ScenePlatformController::LayoutNodeResult Win32ScenePlatformController::dispatchOpenFileDialogLayout(
    Win32ScenePlatformController *controller,
    loka::app::scene::Node *node,
    const LayoutState &state)
{
  if (!controller || !node)
  {
    return LayoutNodeResult(state.width, state.y);
  }
  loka::app::OpenFileDialogNode *dialog = node->asOpenFileDialogNode();
  if (!dialog)
  {
    return LayoutNodeResult(state.width, state.y);
  }
  return controller->layoutOpenFileDialogNode(dialog, state);
}

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
  loka::app::layout::RegisterBuiltinPlatformLayoutHandlers(controller.layoutHandlerRegistry_, &rowMetrics, &gridMetrics);
  controller.leafLayoutHandlerRegistry_.registerHandler(
      loka::app::scene::NodeTypeToken<loka::app::ButtonNode>(),
      &Win32ScenePlatformController::dispatchButtonLayout);
  controller.leafLayoutHandlerRegistry_.registerHandler(
      loka::app::scene::NodeTypeToken<loka::app::EditTextNode>(),
      &Win32ScenePlatformController::dispatchEditTextLayout);
  controller.leafLayoutHandlerRegistry_.registerHandler(
      loka::app::scene::NodeTypeToken<loka::app::PopupMenuNode>(),
      &Win32ScenePlatformController::dispatchPopupMenuLayout);
  controller.leafLayoutHandlerRegistry_.registerHandler(
      loka::app::scene::NodeTypeToken<loka::app::CellNode>(),
      &Win32ScenePlatformController::dispatchCellLayout);
  controller.leafLayoutHandlerRegistry_.registerHandler(
      loka::app::scene::NodeTypeToken<loka::app::TextNode>(),
      &Win32ScenePlatformController::dispatchTextLayout);
  controller.leafLayoutHandlerRegistry_.registerHandler(
      loka::app::scene::NodeTypeToken<loka::app::ImageViewNode>(),
      &Win32ScenePlatformController::dispatchImageViewLayout);
  controller.hostActionHandlerRegistry_.registerHandler(
      loka::app::scene::NodeTypeToken<loka::app::OpenFileDialogNode>(),
      &Win32ScenePlatformController::dispatchOpenFileDialogLayout);
  RegisterWin32ButtonNodeHandler(controller.nodeHandlerRegistry_);
  RegisterWin32TextNodeHandler(controller.nodeHandlerRegistry_);
  RegisterWin32ImageViewNodeHandler(controller.nodeHandlerRegistry_);
  RegisterWin32EditTextNodeHandler(controller.nodeHandlerRegistry_);
  RegisterWin32PopupMenuNodeHandler(controller.nodeHandlerRegistry_);
  RegisterWin32CellNodeHandler(controller.nodeHandlerRegistry_);
  RegisterWin32OpenFileDialogNodeHandler(controller.nodeHandlerRegistry_);
}
