#include "MacBuiltInSupport.hpp"
#include "MacScenePlatformController.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/controls/Cell.hpp"
#include "app/nodes/controls/ScrollBar.hpp"
#include "app/nodes/controls/EditText.hpp"
#include "app/nodes/ImageView.hpp"
#include "app/OpenFileDialog.hpp"
#include "app/nodes/controls/PopupMenu.hpp"
#include "app/nodes/Text.hpp"
#include "app/layout/FallbackControlMetrics.hpp"
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
  MacScenePlatformController::LayoutNodeResult
  DispatchProjectedLayout(MacScenePlatformController *controller,
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
    return MacScenePlatformController::LayoutNodeResult(state.width,
                                                        projected->layoutProjected(controller, projectedState));
  }

  MacScenePlatformController::LayoutNodeResult DispatchTextLayout(MacScenePlatformController *controller,
                                                                  loka::app::scene::Node *node,
                                                                  const MacScenePlatformController::LayoutState &state)
  {
    return DispatchProjectedLayout(controller, node, state);
  }

  MacScenePlatformController::LayoutNodeResult
  DispatchImageViewLayout(MacScenePlatformController *controller,
                          loka::app::scene::Node *node,
                          const MacScenePlatformController::LayoutState &state)
  {
    return DispatchProjectedLayout(controller, node, state);
  }

  MacScenePlatformController::LayoutNodeResult
  DispatchButtonLayout(MacScenePlatformController *controller,
                       loka::app::scene::Node *node,
                       const MacScenePlatformController::LayoutState &state)
  {
    return DispatchProjectedLayout(controller, node, state);
  }

  MacScenePlatformController::LayoutNodeResult
  DispatchEditTextLayout(MacScenePlatformController *controller,
                         loka::app::scene::Node *node,
                         const MacScenePlatformController::LayoutState &state)
  {
    return DispatchProjectedLayout(controller, node, state);
  }

  MacScenePlatformController::LayoutNodeResult
  DispatchPopupMenuLayout(MacScenePlatformController *controller,
                          loka::app::scene::Node *node,
                          const MacScenePlatformController::LayoutState &state)
  {
    return DispatchProjectedLayout(controller, node, state);
  }

  MacScenePlatformController::LayoutNodeResult DispatchCellLayout(MacScenePlatformController *controller,
                                                                  loka::app::scene::Node *node,
                                                                  const MacScenePlatformController::LayoutState &state)
  {
    return DispatchProjectedLayout(controller, node, state);
  }

  MacScenePlatformController::LayoutNodeResult
  DispatchOpenFileDialogLayout(MacScenePlatformController *controller,
                               loka::app::scene::Node *node,
                               const MacScenePlatformController::LayoutState &state)
  {
    return DispatchProjectedLayout(controller, node, state);
  }
} // namespace

namespace
{
  // Mac has no ScrollBar context yet: a known unsupported kind must take the
  // typed-refusal path, not trip the accidental-miss education assert.
  loka::app::scene::RefusedNodeHandler gRefusedMacScrollBar(
      loka::app::scene::NodeTypeToken<loka::app::ScrollBarNode>());
} // namespace

void RegisterMacBuiltInSupport(MacScenePlatformController &controller)
{
  const loka::app::layout::RowLayoutMetrics rowMetrics =
      loka::app::layout::FallbackControlMetrics::rowLayout();
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
  RegisterMacButtonNodeHandler(controller.nodeHandlerRegistry_);
  RegisterMacTextNodeHandler(controller.nodeHandlerRegistry_);
  RegisterMacImageViewNodeHandler(controller.nodeHandlerRegistry_);
  RegisterMacEditTextNodeHandler(controller.nodeHandlerRegistry_);
  RegisterMacPopupMenuNodeHandler(controller.nodeHandlerRegistry_);
  RegisterMacCellNodeHandler(controller.nodeHandlerRegistry_);
  RegisterMacOpenFileDialogNodeHandler(controller.nodeHandlerRegistry_);
  controller.nodeHandlerRegistry_.registerHandler(&gRefusedMacScrollBar);
}
