#include "MacBuiltInSupport.hpp"
#include "MacScenePlatformController.hpp"
#include "app/Button.hpp"
#include "app/Cell.hpp"
#include "app/EditText.hpp"
#include "app/ImageView.hpp"
#include "app/OpenFileDialog.hpp"
#include "app/PopupMenu.hpp"
#include "app/Text.hpp"
#include "app/layout/PlatformBuiltinLayoutHandlers.hpp"
#include "context/MacButtonContext.hpp"
#include "context/MacCellContext.hpp"
#include "context/MacEditTextContext.hpp"
#include "context/MacImageViewContext.hpp"
#include "context/MacOpenFileDialogContext.hpp"
#include "context/MacPopupMenuContext.hpp"
#include "context/MacTextContext.hpp"

namespace
{
  const int kButtonHeight = 32;
  const int kEditTextHeight = 24;
  const int kPopupMenuHeight = 26;
  const int kTextHeight = 20;
  const int kHorizontalSpacing = 12;
  const int kImageFallbackHeightModern = 160;

  MacScenePlatformController::LayoutNodeResult DispatchProjectedLayout(
      MacScenePlatformController *controller,
      loka::app::scene::Node *node,
      const MacScenePlatformController::LayoutState &state)
  {
    if (!controller || !node)
    {
      return MacScenePlatformController::LayoutNodeResult(state.width, state.y);
    }
    loka::app::scene::IProjectedLayoutNode *projected = node->asProjectedLayoutNode();
    if (!projected)
    {
      return MacScenePlatformController::LayoutNodeResult(state.width, state.y);
    }
    loka::app::scene::LayoutState projectedState;
    projectedState.x = static_cast<short>(state.x);
    projectedState.y = static_cast<short>(state.y);
    projectedState.width = static_cast<short>(state.width);
    projectedState.height = static_cast<short>(state.height);
    projectedState.lineHeight = 0;
    projectedState.spacing = 0;
    return MacScenePlatformController::LayoutNodeResult(
        state.width,
        projected->layoutProjected(controller, projectedState));
  }
}

MacScenePlatformController::LayoutNodeResult MacScenePlatformController::dispatchTextLayout(
    MacScenePlatformController *controller,
    loka::app::scene::Node *node,
    const LayoutState &state)
{
  return DispatchProjectedLayout(controller, node, state);
}

MacScenePlatformController::LayoutNodeResult MacScenePlatformController::dispatchImageViewLayout(
    MacScenePlatformController *controller,
    loka::app::scene::Node *node,
    const LayoutState &state)
{
  return DispatchProjectedLayout(controller, node, state);
}

MacScenePlatformController::LayoutNodeResult MacScenePlatformController::dispatchButtonLayout(
    MacScenePlatformController *controller,
    loka::app::scene::Node *node,
    const LayoutState &state)
{
  return DispatchProjectedLayout(controller, node, state);
}

MacScenePlatformController::LayoutNodeResult MacScenePlatformController::dispatchEditTextLayout(
    MacScenePlatformController *controller,
    loka::app::scene::Node *node,
    const LayoutState &state)
{
  return DispatchProjectedLayout(controller, node, state);
}

MacScenePlatformController::LayoutNodeResult MacScenePlatformController::dispatchPopupMenuLayout(
    MacScenePlatformController *controller,
    loka::app::scene::Node *node,
    const LayoutState &state)
{
  return DispatchProjectedLayout(controller, node, state);
}

MacScenePlatformController::LayoutNodeResult MacScenePlatformController::dispatchCellLayout(
    MacScenePlatformController *controller,
    loka::app::scene::Node *node,
    const LayoutState &state)
{
  return DispatchProjectedLayout(controller, node, state);
}

MacScenePlatformController::LayoutNodeResult MacScenePlatformController::dispatchOpenFileDialogLayout(
    MacScenePlatformController *controller,
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

void RegisterMacBuiltInSupport(MacScenePlatformController &controller)
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
      &MacScenePlatformController::dispatchButtonLayout);
  controller.leafLayoutHandlerRegistry_.registerHandler(
      loka::app::scene::NodeTypeToken<loka::app::EditTextNode>(),
      &MacScenePlatformController::dispatchEditTextLayout);
  controller.leafLayoutHandlerRegistry_.registerHandler(
      loka::app::scene::NodeTypeToken<loka::app::PopupMenuNode>(),
      &MacScenePlatformController::dispatchPopupMenuLayout);
  controller.leafLayoutHandlerRegistry_.registerHandler(
      loka::app::scene::NodeTypeToken<loka::app::CellNode>(),
      &MacScenePlatformController::dispatchCellLayout);
  controller.leafLayoutHandlerRegistry_.registerHandler(
      loka::app::scene::NodeTypeToken<loka::app::TextNode>(),
      &MacScenePlatformController::dispatchTextLayout);
  controller.leafLayoutHandlerRegistry_.registerHandler(
      loka::app::scene::NodeTypeToken<loka::app::ImageViewNode>(),
      &MacScenePlatformController::dispatchImageViewLayout);
  controller.hostActionHandlerRegistry_.registerHandler(
      loka::app::scene::NodeTypeToken<loka::app::OpenFileDialogNode>(),
      &MacScenePlatformController::dispatchOpenFileDialogLayout);
  RegisterMacButtonNodeHandler(controller.nodeHandlerRegistry_);
  RegisterMacTextNodeHandler(controller.nodeHandlerRegistry_);
  RegisterMacImageViewNodeHandler(controller.nodeHandlerRegistry_);
  RegisterMacEditTextNodeHandler(controller.nodeHandlerRegistry_);
  RegisterMacPopupMenuNodeHandler(controller.nodeHandlerRegistry_);
  RegisterMacCellNodeHandler(controller.nodeHandlerRegistry_);
  RegisterMacOpenFileDialogNodeHandler(controller.nodeHandlerRegistry_);
}
