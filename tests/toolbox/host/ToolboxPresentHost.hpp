#ifndef LOKA_TEST_TOOLBOX_PRESENT_HOST_HPP
#define LOKA_TEST_TOOLBOX_PRESENT_HOST_HPP
// This fixture has no file-dialog contexts; their real native seam is in
// ToolboxDialogPresentHostTests.hpp.
static loka::app::FileChooserResult RunToolboxFileDialog(const loka::app::FileDialogOptions &)
{ LOKA_VERIFY(false); return loka::app::FileChooserResult(); }
static void DeliverOpenFileDialogResult(loka::app::scene::NodeState<loka::app::FileChooserResult>,
                                       loka::core::EmitterState *, const loka::app::FileChooserResult &)
{ LOKA_VERIFY(false); }
/** The real Toolbox completion with fixture-owned native neighbors. */
class ToolboxApp : public WindowAdmissionTestApp
{
public:
  struct Cursor { void reconcile() {} } cursorOwner_;
  bool running_;
  explicit ToolboxApp(Window &window) : WindowAdmissionTestApp(window), running_(true) {}
  void present(ActivationPhase, loka::core::Operation &turn);
  bool hasPendingDialogs() const;
};
#include "ToolboxPresent.cpp"
#endif
