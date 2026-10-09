#ifndef LOKA_TOOLBOX_OPEN_FILE_DIALOG_CONTEXT_HPP
#define LOKA_TOOLBOX_OPEN_FILE_DIALOG_CONTEXT_HPP

#include "app/scene/projection/NativeNodeContext.hpp"
#include "app/OpenFileDialog.hpp"
#include "ToolboxPendingDialogs.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      class PlatformNodeHandlerRegistry;
    }
  } // namespace app
} // namespace loka

class ToolboxOpenFileDialogContext : public loka::app::scene::NativeNodeContext
{
public:
  ToolboxOpenFileDialogContext(loka::app::OpenFileDialogNode *node, ToolboxPendingDialogs *pending);
  virtual void onPropsApplied();
  /** Attach-time read (late-subscriber rule): enrollment from the current
      fact, called by the installing handler right after setContext. */
  void readLifecycleFactOnAttach();
  virtual void onFactChanged(loka::app::scene::NodeLifecycleFact previous,
                             loka::app::scene::NodeLifecycleFact next);
private:
  friend class ToolboxPendingDialogs;
  bool take(loka::app::OpenFileDialogProps &out);
  /** Borrowed from the owning window controller. */
  ToolboxPendingDialogs *pending_;
  ToolboxDialogEnrollment enrollment_;
  void captureProps();
  void applyAttachedPresentation();
  void applyDetachedPresentation();

  loka::app::OpenFileDialogNode *node_;
  loka::app::OpenFileDialogProps props_;
  loka::app::OpenFileDialogPresentationPhase presentation_;
};

bool RegisterToolboxOpenFileDialogNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry);

#endif // LOKA_TOOLBOX_OPEN_FILE_DIALOG_CONTEXT_HPP
