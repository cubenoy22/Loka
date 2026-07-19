#ifndef LOKA_MAC_OPEN_FILE_DIALOG_CONTEXT_HPP
#define LOKA_MAC_OPEN_FILE_DIALOG_CONTEXT_HPP

#include "app/scene/projection/NativeNodeContext.hpp"
#include "app/OpenFileDialog.hpp"
#include "core/String.hpp"

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

class MacOpenFileDialogContext : public loka::app::scene::NativeNodeContext
{
public:
  MacOpenFileDialogContext(void *parentView, loka::app::OpenFileDialogNode *node);
  virtual ~MacOpenFileDialogContext();
  /** Attach-time read (late-subscriber rule): presentation from the current
      fact, called by the installing handler right after setContext. */
  void readLifecycleFactOnAttach();
  virtual void onFactChanged(loka::app::scene::NodeLifecycleFact previous,
                             loka::app::scene::NodeLifecycleFact next);
  void presentIfNeeded();
  void presentDeferred();

private:
  void applyAttachedPresentation();
  void applyDetachedPresentation();
  struct NativeDialogSession;

  void presentDialog();
  void setResult(const loka::app::FileChooserResult &result);
  void disposeDialog();
  NativeDialogSession *detachDialogIfActive(NativeDialogSession *dialog);

  void *parentView_;
  loka::app::OpenFileDialogNode *node_;
  loka::core::MutableState<loka::app::FileChooserResult> *resultState_;
  loka::core::EmitterState *onResult_;
  loka::app::OpenFileDialogPresentationPhase presentation_;
  void *deferredPresenter_;
  NativeDialogSession *dialog_;
};

void RegisterMacOpenFileDialogNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry);

#endif // LOKA_MAC_OPEN_FILE_DIALOG_CONTEXT_HPP
