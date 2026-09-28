#ifndef LOKA_MAC_INPUT_DOOR_HPP
#define LOKA_MAC_INPUT_DOOR_HPP

#include "MacScenePlatformController.hpp"
#include "MacWindow.hpp"
#include "context/MacButtonContext.hpp"
#include "context/MacEditTextContext.hpp"
#include "context/MacPopupMenuContext.hpp"
#include "context/MacScrollViewContext.hpp"
#include "context/MacCellContext.hpp"
#include "context/MacTextEditorContext.hpp"

/** Complete synchronous macOS input operations, parallel to Win32InputDoor.
    The borrow restores without pumping; MacApp::flushInvalidationsTick remains
    the completion. Adapters never borrow a receiver after invocation. */
class MacInputDoor
{
  typedef loka::app::scene::detail::InputInvocation Invocation;

public:
  static void buttonPress(MacButtonContext &c)
  {
    Invocation(*c.controller())(c, &MacButtonContext::handlePress);
  }
  static void editTextChange(MacEditTextContext &c)
  {
    Invocation(*c.controller())(c, &MacEditTextContext::handleTextDidChange);
  }
  static void popupChange(MacPopupMenuContext &c)
  {
    Invocation(*c.controller())(c, &MacPopupMenuContext::handleSelectionChange);
  }
  static void scrollBoundsChange(MacScrollViewContext &c)
  {
    Invocation(*c.controller())(c, &MacScrollViewContext::publishClipViewBoundsOrigin);
  }
  static void cellClick(MacCellContext &c, void *event)
  {
    Invocation(*c.controller())(c, &MacCellContext::handleClick, event);
  }
  static void textEditorChange(MacTextEditorContext &c, MacTextEditorContext::TextObservation source, std::size_t caret)
  {
    Invocation(*c.controller())(c, &MacTextEditorContext::handleTextDidChange, source, caret);
  }
  static void textEditorSelection(MacTextEditorContext &c)
  {
    Invocation(*c.controller())(c, &MacTextEditorContext::handleSelectionDidChange);
  }
  static void textEditorCapture(MacTextEditorContext &c)
  {
    Invocation(*c.controller())(c, &MacTextEditorContext::captureSelection);
  }
  static void textEditorRestore(MacTextEditorContext &c)
  {
    Invocation(*c.controller())(c, &MacTextEditorContext::restoreCommittedProjection);
  }
  static void textEditorHighlights(MacTextEditorContext &c)
  {
    Invocation(*c.controller())(c, &MacTextEditorContext::applyHighlights);
  }
  static void windowResize(MacWindow &w)
  {
    if (w.scenePlatformController_)
      Invocation(*w.scenePlatformController_)(w, &MacWindow::handleWindowDidResize);
    else
      w.handleWindowDidResize(); // N5: creation/teardown without a controller.
  }
  static void windowMove(MacWindow &w)
  {
    if (w.scenePlatformController_)
      Invocation(*w.scenePlatformController_)(w, &MacWindow::handleWindowDidMove);
    else
      w.handleWindowDidMove(); // N5: creation/teardown without a controller.
  }
  static void pendingRelayout(MacScenePlatformController &c)
  {
    Invocation(c).operator()(c, &MacScenePlatformController::handlePendingRelayout);
  }
};
#endif
